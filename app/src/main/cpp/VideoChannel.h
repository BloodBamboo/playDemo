//
// Created by 12 on 2020/4/9.
//

#ifndef PLAYDEMO_VIDEOCHANNEL_H
#define PLAYDEMO_VIDEOCHANNEL_H


#include "BaseChannel.h"
#include "AudioChannel.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
};

/**
 * 1、解码
 * 2、播放
 */
typedef void (*RenderFrameCallback)(uint8_t *, int, int, int);

class VideoChannel : public BaseChannel {
private:
    pthread_t _pid_decode;
    pthread_t _pid_render;
    SwsContext *_swsContext = nullptr;
    RenderFrameCallback _callback;
    int _fps = 0;
    AudioChannel * _audioChannel = nullptr;
    bool _isRtmp = false;

public:
    VideoChannel(int id, AVCodecContext *avCodecContext,
            AVRational time_base, int fps, JavaCallHelper *javaCallHelper,
                 bool isRtmp);

    ~VideoChannel();

    //解码+播放
    void play();
    void stop();
    void pause();
    void decode();

    void render();

    void setRenderFrameCallback(RenderFrameCallback callback);

    void setAudioChannel(AudioChannel *pChannel);

};


#endif //PLAYDEMO_VIDEOCHANNEL_H
