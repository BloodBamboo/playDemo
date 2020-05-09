//
// Created by 12 on 2020/4/9.
//

#ifndef PLAYDEMO_AUDIOCHANNEL_H
#define PLAYDEMO_AUDIOCHANNEL_H

#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

extern "C" {
#include <libswresample/swresample.h>
};

class AudioChannel : public BaseChannel {
private:
    pthread_t _pid_audio_decode;
    pthread_t _pid_audio_play;

    /**
    * OpenSL ES
    */
    // 引擎与引擎接口
    SLObjectItf _engineObject = nullptr;
    SLEngineItf _engineInterface = nullptr;
    //混音器
    SLObjectItf _outputMixObject = nullptr;
    //播放器
    SLObjectItf _bqPlayerObject = nullptr;
    //播放器接口
    SLPlayItf _bqPlayerInterface = nullptr;

    SLAndroidSimpleBufferQueueItf _bqPlayerBufferQueueInterface = nullptr;


    //重采样
    SwrContext *_swrContext = nullptr;

public:
    //数据大小缓存区
    uint8_t *data = nullptr;
//输出通道数
    int out_channels;
//  输出  比特率
    int out_samplesize;
//输出采样率
    int out_sample_rate;


    AudioChannel(int id, AVCodecContext *avCodecContext, AVRational time_base, JavaCallHelper *javaCallHelper);

    ~AudioChannel();

    void play();

    void stop();
    void pause();
    void decode();

    void threadPlay();

// 声卡每次处理固定数据，处理完再通过回调获取新的数据
    int getPcm();
};


#endif //PLAYDEMO_AUDIOCHANNEL_H
