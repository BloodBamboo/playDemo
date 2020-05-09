//
// Created by 12 on 2020/4/9.
//
#include "JavaCallHelper.h"
#include "AudioChannel.h"
#include "VideoChannel.h"
#include <pthread.h>
#include "macro.h"

extern "C" {
#include "libavformat/avformat.h"
}


class DemoPlay {
public:
    AudioChannel *audioChannel = nullptr;
    VideoChannel *videoChannel = nullptr;
    pthread_t pid;
    pthread_t pid_play;
    AVFormatContext *avFormatContext = nullptr;

    DemoPlay(JavaCallHelper *javaCallHelp, const char *dataSource);

    ~DemoPlay();

    void prepare();

    void threadPrepare();

    void start();

    void pause();

    void threadStart();

    void stop();
    void setRenderFrameCallback(RenderFrameCallback callback);

    int getDuration() {
        return _duration;
    }

    void seek(int i);
private:
    pthread_t _pid_stop;
    char *_data_source = nullptr;
    JavaCallHelper *_javaCallHelp = nullptr;
    RenderFrameCallback _callback = nullptr;
    bool _isPlaying = false;
    bool _isPrepare = false;
    int _duration = 0;
    bool _isSeek = false;
    bool _isPause = false;
    pthread_mutex_t _seekMutex;
    pthread_cond_t _p_play_cond;
};
