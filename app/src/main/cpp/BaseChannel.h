//
// Created by 12 on 2020/4/10.
//

#ifndef PLAYDEMO_BASECHANNEL_H
#define PLAYDEMO_BASECHANNEL_H

#include "SafeQueue.cpp"
#include "JavaCallHelper.h"

extern "C" {
#include "include/libavcodec/avcodec.h"
#include "include/libavutil/frame.h"
#include <libavutil/time.h>
};

class BaseChannel {
public:
    //编码数据包队列
    SafeQueue<AVPacket *> packets;
    //解码数据包队列
    SafeQueue<AVFrame *> frames;
    int id;
    bool isPlaying;
    AVCodecContext *avCodecContext;
//    用来计算pts
    AVRational time_base;
    double clock = 0;

    JavaCallHelper *javaCallHelper;

    bool isPause = false;

    pthread_mutex_t p_decode;
    pthread_cond_t p_decode_cond;
    pthread_mutex_t p_play;
    pthread_cond_t p_play_cond;

    BaseChannel(int id, AVCodecContext *avCodecContext, AVRational time_base,
                JavaCallHelper *javaCallHelper) :
            id(id), avCodecContext(avCodecContext), time_base(time_base),
            javaCallHelper(javaCallHelper) {
        frames.setReleaseCallback(releaseAvFrame);
        packets.setReleaseCallback(releaseAvPacket);
        pthread_cond_init(&p_decode_cond, nullptr);
        pthread_cond_init(&p_play_cond, nullptr);
        pthread_mutex_init(&p_decode, nullptr);
        pthread_mutex_init(&p_play, nullptr);
    }

    ~BaseChannel() {
        packets.clear();
        frames.clear();
        if (avCodecContext) {
            avcodec_close(avCodecContext);
            avcodec_free_context(&avCodecContext);
            avCodecContext = nullptr;
        }
        pthread_cond_destroy(&p_decode_cond);
        pthread_cond_destroy(&p_play_cond);
        pthread_mutex_destroy(&p_decode);
        pthread_mutex_destroy(&p_play);
    }

    //纯虚方法 相当于 抽象方法
    virtual void play() = 0;

    virtual void stop() = 0;

    virtual void pause() = 0;

    void clear() {
        packets.clear();
        frames.clear();
    }

    void stopWork() {
        packets.setWork(0);
        frames.setWork(0);
    }

    void startWork() {
        packets.setWork(1);
        frames.setWork(1);
    }

    /**
     * 释放 AVPacket
     * @param packet
     */
    static void releaseAvPacket(AVPacket **packet) {
        if (packet) {
            av_packet_free(packet);
            //为什么用指针的指针？
            // 指针的指针能够修改传递进来的指针的指向
            *packet = nullptr;
        }
    }

    static void releaseAvFrame(AVFrame **frame) {
        if (frame) {
            av_frame_free(frame);
            //为什么用指针的指针？
            // 指针的指针能够修改传递进来的指针的指向
            *frame = nullptr;
        }
    }
};


#endif //PLAYDEMO_BASECHANNEL_H
