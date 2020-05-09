//
// Created by 12 on 2020/4/9.
//

#include <queue>
#include <pthread.h>
#include "macro.h"

using namespace std;

template<typename T>
class SafeQueue {
private:
    queue<T> _queue;
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;
    int work = 0;

    typedef void (*ReleaseCallback)(T *);

    typedef void (*SyncHandle)(queue<T> &);

    ReleaseCallback releaseCallback;
    SyncHandle syncHandle;
public:
    SafeQueue() {
        pthread_mutex_init(&_mutex, nullptr);
        pthread_cond_init(&_cond, nullptr);
    }

    ~SafeQueue() {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond);
    }


    void push(T t) {
        pthread_mutex_lock(&_mutex);
        //这里work一定不能去掉，不然在停止的时候，会因为条件满足导致一直等待，导致线程无法结束
        while (work && _queue.size() >= 100) {
            pthread_cond_wait(&_cond, &_mutex);
        }
        if (!work) {
            releaseCallback(&t);
        } else {
            _queue.push(t);
            pthread_cond_signal(&_cond);
        }
        pthread_mutex_unlock(&_mutex);
    }

    int pop(T &value) {
        int ret = 0;
        pthread_mutex_lock(&_mutex);
        //在多核处理器下 由于竞争可能虚假唤醒 包括jdk也说明了
//这里work一定不能去掉，不然在停止的时候，会因为条件满足导致一直等待，导致线程无法结束
        while (work && _queue.empty()) {
            pthread_cond_wait(&_cond, &_mutex);
        }

        if (!work) {
            releaseCallback(&value);
        } else {
            if (!_queue.empty()) {
                value = _queue.front();
                _queue.pop();
                pthread_cond_signal(&_cond);
                ret = 1;
            }
        }
        pthread_mutex_unlock(&_mutex);
        return ret;
    }

    void setWork(int work) {
        pthread_mutex_lock(&_mutex);
        this->work = work;
        pthread_cond_broadcast(&_cond);
        pthread_mutex_unlock(&_mutex);
    }

    int empty() {
        return _queue.empty();
    }

    int size() {
        return _queue.size();
    }

    void clear() {
        pthread_mutex_lock(&_mutex);
        int size = _queue.size();
        for (int i = 0; i < size; ++i) {
            T value = _queue.front();
            releaseCallback(&value);
            _queue.pop();
        }
        pthread_mutex_unlock(&_mutex);
    }

    //音视频同步时调用，用来处理丢包逻辑
    void sync() {
        pthread_mutex_lock(&_mutex);
        syncHandle(_queue);
        pthread_mutex_unlock(&_mutex);
    }

    void setReleaseCallback(ReleaseCallback r) {
        releaseCallback = r;
    }

    void setSyncHandle(SyncHandle s) {
        syncHandle = s;
    }
};