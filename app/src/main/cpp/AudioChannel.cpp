//
// Created by 12 on 2020/4/9.
//

#include "AudioChannel.h"

// 声明并且实现
void *audio_decode(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->decode();
    return 0;
}

void *audio_play(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->threadPlay();
    return 0;
}

AudioChannel::AudioChannel(int id, AVCodecContext *avCodecContext, AVRational time_base,
                           JavaCallHelper *javaCallHelper)
        : BaseChannel(id, avCodecContext, time_base, javaCallHelper) {
//    双通道立体声
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    out_sample_rate = 44100;
//    比特率 16比特
    out_samplesize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    //44100个16位 44100 * 2
    // 44100*(双声道)*(16位)
    data = static_cast<uint8_t *>(malloc(out_sample_rate * out_channels * out_samplesize));
//    初始化缓冲区为0
    memset(data, 0, out_sample_rate * out_channels * out_samplesize);
}

AudioChannel::~AudioChannel() {
    if (data) {
        free(data);
        data = nullptr;
    }

//    stop();

}

void AudioChannel::play() {
    isPlaying = true;
    //设置为工作状态
    packets.setWork(1);
    frames.setWork(1);

    //0+输出声道+输出采样位+输出采样率+  输入的3个参数
    _swrContext = swr_alloc_set_opts(0, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate,
                                     avCodecContext->channel_layout, avCodecContext->sample_fmt,
                                     avCodecContext->sample_rate, 0, 0);

    //初始化
    swr_init(_swrContext);

    //1 、解码
    pthread_create(&_pid_audio_decode, 0, audio_decode, this);
    //2、 播放
    pthread_create(&_pid_audio_play, 0, audio_play, this);
}

void AudioChannel::decode() {
    AVPacket *packet = nullptr;
    pthread_mutex_lock(&p_decode);
    while (isPlaying) {
        while (isPause) {
//            av_usleep(1000 * 10);
            pthread_cond_wait(&p_decode_cond, &p_decode);
        }
        //取出一个数据包
        int ret = packets.pop(packet);
        if (!isPlaying) {
            break;
        }
        //取出失败
        if (!ret) {
            LOGE("AudioChannel 取出失败");
            continue;
        }
        //把包丢给解码器
        ret = avcodec_send_packet(avCodecContext, packet);
        releaseAvPacket(&packet);
        //重试
        if (ret != 0) {
            break;
        }
        //代表了一个图像 (将这个图像先输出来)
        AVFrame *frame = av_frame_alloc();
        //从解码器中读取 解码后的数据包 AVFrame
        ret = avcodec_receive_frame(avCodecContext, frame);
        //需要更多的数据才能够进行解码
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret != 0) {
            break;
        }
        //再开一个线程 来播放 (流畅度)
        frames.push(frame);
    }
    releaseAvPacket(&packet);
    pthread_mutex_unlock(&p_decode);
}

//返回获取的pcm数据大小
int AudioChannel::getPcm() {
    pthread_mutex_lock(&p_play);
    while (isPause) {
//        av_usleep(1000 * 10);
        pthread_cond_wait(&p_play_cond, &p_play);
    }
    int data_size = 0;
    AVFrame *frame;
    while (isPlaying) {
        int ret = frames.pop(frame);
        if (!isPlaying) {
            if (ret) {
                releaseAvFrame(&frame);
            }
            return data_size;
        }
        if (!ret) {
            continue;
        }

        //48000HZ 8位 =》 44100 16位,把音频数据转换成设置的固定格式 ：44100采样率 16bit 2通道
        //重采样
        // 假设我们输入了10个数据 ，swrContext转码器 这一次处理了8个数据
        // 那么如果不加delays(上次没处理完的数据) , 积压
        int64_t delays = swr_get_delay(_swrContext, frame->sample_rate);
        // 将 nb_samples 个数据 由 sample_rate采样率转成 44100 后 返回多少个数据
        // 10  个 48000 = nb 个 44100
        // AV_ROUND_UP : 向上取整 1.1 = 2
        int64_t max_samples = av_rescale_rnd(delays + frame->nb_samples, out_sample_rate,
                                             frame->sample_rate, AV_ROUND_UP);
        //上下文+输出缓冲区+输出缓冲区能接受的最大数据量+输入数据+输入数据个数
        //返回 每一个声道的输出数据
        int samples = swr_convert(_swrContext, &data, max_samples, (const uint8_t **) frame->data,
                                  frame->nb_samples);
        //获得   samples 个   * 2 声道 * 2字节（16位）
        data_size = samples * out_samplesize * out_channels;
        // 获得 相对播放这一段数据的秒数
        clock = frame->pts * av_q2d(time_base);
        break;
    }
    releaseAvFrame(&frame);
    pthread_mutex_unlock(&p_play);
    return data_size;
}

//音频数据回调
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *args) {

    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    //获得pcm 数据 多少个字节 data
    if (audioChannel->isPlaying) {
        int dataSize = audioChannel->getPcm();
        if (dataSize > 0) {
            // 接收16位数据
            (*bq)->Enqueue(bq, audioChannel->data, dataSize);
        }
    }
}

void AudioChannel::threadPlay() {
    /**
        * 1、创建引擎并获取引擎接口
        */
    SLresult result;
    // 1.1 创建引擎 SLObjectItf engineObject
    result = slCreateEngine(&_engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 1.2 初始化引擎  init
    result = (*_engineObject)->Realize(_engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 1.3 获取引擎接口SLEngineItf engineInterface
    result = (*_engineObject)->GetInterface(_engineObject, SL_IID_ENGINE,
                                            &_engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    /**
     * 2、设置混音器
     */
    // 2.1 创建混音器SLObjectItf outputMixObject
    result = (*_engineInterface)->CreateOutputMix(_engineInterface, &_outputMixObject, 0,
                                                  0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 2.2 初始化混音器outputMixObject
    result = (*_outputMixObject)->Realize(_outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    /**
     * 3、创建播放器
     */
    //3.1 配置输入声音信息
    //创建buffer缓冲类型的队列 2个队列
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            2};
    //pcm数据格式
    //pcm+2(双声道)+44100(采样率)+ 16(采样位)+16(数据的大小)+LEFT|RIGHT(双声道)+小端数据
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN};

    //数据源 将上述配置信息放到这个数据源中
    SLDataSource slDataSource = {&android_queue, &pcm};

    //3.2  配置音轨(输出)
    //设置混音器
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, _outputMixObject};
    SLDataSink audioSnk = {&outputMix, NULL};
    //需要的接口  操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    //3.3 创建播放器
    (*_engineInterface)->CreateAudioPlayer(_engineInterface, &_bqPlayerObject, &slDataSource,
                                           &audioSnk, 1,
                                           ids, req);
    //初始化播放器
    (*_bqPlayerObject)->Realize(_bqPlayerObject, SL_BOOLEAN_FALSE);

    //得到接口后调用  获取Player接口
    (*_bqPlayerObject)->GetInterface(_bqPlayerObject, SL_IID_PLAY, &_bqPlayerInterface);


    /**
     * 4、设置播放回调函数
     */
    //获取播放器队列接口
    (*_bqPlayerObject)->GetInterface(_bqPlayerObject, SL_IID_BUFFERQUEUE,
                                     &_bqPlayerBufferQueueInterface);
    //设置回调
    (*_bqPlayerBufferQueueInterface)->RegisterCallback(_bqPlayerBufferQueueInterface,
                                                       bqPlayerCallback, this);
    /**
     * 5、设置播放状态
     */
    (*_bqPlayerInterface)->SetPlayState(_bqPlayerInterface, SL_PLAYSTATE_PLAYING);
    /**
     * 6、手动激活一下这个回调
     */
    bqPlayerCallback(_bqPlayerBufferQueueInterface, this);
}

void AudioChannel::stop() {
    isPlaying = false;
    javaCallHelper = nullptr;
    clock = 0;
    packets.setWork(0);
    frames.setWork(0);
    pthread_join(_pid_audio_decode, nullptr);
    pthread_join(_pid_audio_play, nullptr);
    avcodec_flush_buffers(avCodecContext);
    if (_swrContext) {
        swr_free(&_swrContext);
        _swrContext = nullptr;
    }
    //释放播放器
    if (_bqPlayerObject) {
        (*_bqPlayerObject)->Destroy(_bqPlayerObject);
        _bqPlayerObject = nullptr;
        _bqPlayerBufferQueueInterface = nullptr;
        _bqPlayerInterface = nullptr;
    }

    //释放混音器
    if (_outputMixObject) {
        (*_outputMixObject)->Destroy(_outputMixObject);
        _outputMixObject = nullptr;
    }

    //释放引擎
    if (_engineObject) {
        (*_engineObject)->Destroy(_engineObject);
        _engineObject = nullptr;
        _engineInterface = nullptr;
    }
    LOGE("AudioChannel::stop");
}

void AudioChannel::pause() {
    if (isPause) {
        isPause = false;
        pthread_cond_broadcast(&p_decode_cond);
        pthread_cond_broadcast(&p_play_cond);
    } else {
        isPause = true;
    }
}