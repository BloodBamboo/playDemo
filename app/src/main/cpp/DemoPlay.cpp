//
// Created by 12 on 2020/4/9.
//


#include "DemoPlay.h"


DemoPlay::DemoPlay(JavaCallHelper *javaCallHelp, const char *dataSource) {
    _javaCallHelp = javaCallHelp;
    //防止 dataSource参数 指向的内存被释放
    _data_source = new char[strlen(dataSource) + 1];
    strcpy(_data_source, dataSource);
    pthread_mutex_init(&_seekMutex, nullptr);
    pthread_cond_init(&_p_play_cond, nullptr);
}

DemoPlay::~DemoPlay() {
    //释放
    DELETE(_data_source);
    _javaCallHelp = nullptr;
    pthread_mutex_destroy(&_seekMutex);
    pthread_cond_destroy(&_p_play_cond);
}

void *task(void *args) {
    DemoPlay *demoPlay = static_cast<DemoPlay *>(args);
    demoPlay->threadPrepare();
    return 0;
}

void DemoPlay::prepare() {
    _isPrepare = true;
    //创建一个线程
    pthread_create(&pid, nullptr, task, this);
}


void DemoPlay::threadPrepare() {
    // 初始化网络 让ffmpeg能够使用网络
    avformat_network_init();
    avFormatContext = avformat_alloc_context();
    //1、打开媒体地址(文件地址、直播地址)
    // AVFormatContext  包含了 视频的 信息(宽、高等)
    // 第3个参数： 指示打开的媒体格式(传NULL，ffmpeg就会自动推到出是mp4还是flv)
//    AVDictionary *options = nullptr;
    //设置超时时间 微妙 超时时间5秒
//    av_dict_set(&options, "timeout", "5000000", 0);
    int ret = avformat_open_input(&avFormatContext, _data_source, nullptr, nullptr);
//    av_dict_free(&options);
    //ret不为0表示 打开媒体失败
    //文件路径不对 手机没网
    if (ret != 0) {
        LOGE("打开媒体失败:%s", av_err2str(ret));
        if (_isPrepare) {
            _javaCallHelp->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
        }
        return;
    }
    //2、查找媒体中的 音视频流 (给 contxt里的 streams等成员赋)
    ret = avformat_find_stream_info(avFormatContext, nullptr);
    // 小于0 则失败
    if (ret < 0) {
        LOGE("查找流失败:%s", av_err2str(ret));
        if (_isPrepare) {
            _javaCallHelp->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }
    //视频时长（单位：微秒us，转换为秒需要除以1000000）
    _duration = avFormatContext->duration / 1000000;
    //nb_streams :几个流(几段视频/音频)
    for (int i = 0; i < avFormatContext->nb_streams; i++) {
        //可能代表是一个视频 也可能代表是一个音频
        AVStream *avStream = avFormatContext->streams[i];
        //包含了 解码 这段流 的各种参数信息(宽、高、码率、帧率)
        AVCodecParameters *avCodecParameters = avStream->codecpar;
        //无论视频还是音频都需要干的一些事情（获得解码器）
        // 1、通过 当前流 使用的 编码方式，查找解码器
        AVCodec *dec = avcodec_find_decoder(avCodecParameters->codec_id);
        if (dec == NULL) {
            LOGE("查找解码器失败:%s", av_err2str(ret));
            if (_isPrepare) {
                _javaCallHelp->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }
            return;
        }
        //2、获得解码器上下文
        AVCodecContext *avCodecContext = avcodec_alloc_context3(dec);
        if (avCodecContext == NULL) {
            LOGE("创建解码上下文失败:%s", av_err2str(ret));
            if (_isPrepare) {
                _javaCallHelp->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }

            return;
        }
        //3、设置上下文内的一些参数 (context->width)
        ret = avcodec_parameters_to_context(avCodecContext, avCodecParameters);
        //失败
        if (ret < 0) {
            LOGE("设置解码上下文参数失败:%s", av_err2str(ret));
            if (_isPrepare) {
                _javaCallHelp->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }
        // 4、打开解码器
        ret = avcodec_open2(avCodecContext, dec, nullptr);
        if (ret != 0) {
            LOGE("打开解码器失败:%s", av_err2str(ret));
            if (_isPrepare) {
                _javaCallHelp->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }
        // 单位
        AVRational time_base = avStream->time_base;
        if (avCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            //音频
            if (_isPrepare) {
                audioChannel = new AudioChannel(i, avCodecContext, time_base, _javaCallHelp);
            }
        } else if (avCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            //1
            //帧率： 单位时间内 需要显示多少个图像
            AVRational frame_rate = avStream->avg_frame_rate;
            int fps = av_q2d(frame_rate);
            //视频
            if (_isPrepare) {
                videoChannel = new VideoChannel(i, avCodecContext, time_base, fps, _javaCallHelp,
                                                _duration == 0 ? true : false);
                videoChannel->setRenderFrameCallback(_callback);
            }
        }
    }
    //没有音视频  (很少见)
    if (!audioChannel && !videoChannel) {
        LOGE("没有音视频");
        if (_isPrepare) {
            _javaCallHelp->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }
        return;
    }
    // 准备完了 通知java 你随时可以开始播放
    if (_isPrepare) {
        _javaCallHelp->onPrepare(THREAD_CHILD);
        _isPrepare = false;
    }
}

void *playTask(void *args) {
    DemoPlay *demoPlay = static_cast<DemoPlay *>(args);
    demoPlay->threadStart();
    return 0;
}

void DemoPlay::start() {
    // 正在播放
    _isPlaying = true;
//    isEnd = false;
    if (audioChannel) {
        //启动声音的解码与播放
        audioChannel->play();
    }
    if (videoChannel) {
        // 启动播放
        videoChannel->setAudioChannel(audioChannel);
        videoChannel->play();
    }

    pthread_create(&pid_play, nullptr, playTask, this);
}

void DemoPlay::threadStart() {
    //1、读取媒体数据包(音视频数据包)
    int ret;
    LOGE("开始播放");
    while (_isPlaying) {
        //锁住formatContext
        pthread_mutex_lock(&_seekMutex);
        while (_isPause) {
//            av_usleep(1000 * 10);
            pthread_cond_wait(&_p_play_cond, &_seekMutex);
        }

        //非常重要 在不把包放入到队列里时一定要释放，不然，最后会产生溢出，还不易查找，还无法释放内存
        AVPacket *avPacket = av_packet_alloc();
        ret = av_read_frame(avFormatContext, avPacket);
        pthread_mutex_unlock(&_seekMutex);
        //=0成功 其他:失败
        if (ret == 0) {
            //stream_index 这一个流的一个序号
            if (audioChannel && audioChannel->id == avPacket->stream_index) {
                audioChannel->packets.push(avPacket);
            } else if (videoChannel && videoChannel->id == avPacket->stream_index) {
                videoChannel->packets.push(avPacket);
            }
        } else if (ret == AVERROR_EOF) {
            //非常重要 在不把包放入到队列里时一定要释放，不然，最后会产生溢出，还不易查找，还无法释放内存
            if (avPacket) {
                av_packet_free(&avPacket);
                //为什么用指针的指针？
                // 指针的指针能够修改传递进来的指针的指向
                avPacket = nullptr;
            }
            if (audioChannel->packets.empty() && audioChannel->frames.empty() &&
                videoChannel->packets.empty() && videoChannel->frames.empty()) {
                LOGE("播放完成");
                _javaCallHelp->onPlayEnd(THREAD_CHILD);
                break;
            }
            //非常重要 在不把包放入到队列里时一定要释放，不然，最后会产生溢出，还不易查找，还无法释放内存
            //为什么这里要让它继续循环 而不是sleep
            //如果是做直播 ，可以sleep
            //如果要支持点播(播放本地文件） seek 后退
        } else {
            //非常重要 在不把包放入到队列里时一定要释放，不然，最后会产生溢出，还不易查找，还无法释放内存
            if (avPacket) {
                av_packet_free(&avPacket);
                //为什么用指针的指针？
                // 指针的指针能够修改传递进来的指针的指向
                avPacket = nullptr;
            }
            LOGE("%d", ret);
            break;
        }
    }
    stop();
//    _isPause = false;
//    _isPlaying = false;

//    audioChannel->stop();
//    videoChannel->stop();
}

void DemoPlay::setRenderFrameCallback(RenderFrameCallback callback) {
    _callback = callback;
}

void *aync_stop(void *args) {
    LOGE("aync_stop");
    DemoPlay *play = static_cast<DemoPlay *>(args);
    //   等待prepare结束
    pthread_join(play->pid, nullptr);
    // 保证 start线程结束
    pthread_join(play->pid_play, nullptr);
    if (play->videoChannel) {
        play->videoChannel->stop();
    }
    if (play->audioChannel) {
        play->audioChannel->stop();
    }

    DELETE(play->videoChannel);
    DELETE(play->audioChannel);
    // 这时候释放就不会出现问题了
    if (play->avFormatContext) {
        //先关闭读取 (关闭fileintputstream)
        avformat_close_input(&play->avFormatContext);
        avformat_free_context(play->avFormatContext);
        play->avFormatContext = nullptr;
    }
    DELETE(play);
    LOGE("释放");
    return 0;
}

void DemoPlay::stop() {
    if (!_isPlaying && !_isPrepare) {
        LOGE("no stop");
        return;
    }
    if (_isPause) {
        if (audioChannel) {
            audioChannel->pause();
        }
        if (videoChannel) {
            videoChannel->pause();
        }

        _isPause = false;
        pthread_cond_broadcast(&_p_play_cond);
    }

    _isPlaying = false;
    _isPrepare = false;
    _javaCallHelp = nullptr;

    // formatContext
    pthread_create(&_pid_stop, 0, aync_stop, this);
}

void DemoPlay::seek(int i) {
    //进去必须 在0- duration 范围之类
    if (i < 0 || i >= _duration) {
        return;
    }
    if (!audioChannel && !videoChannel) {
        return;
    }
    if (!avFormatContext) {
        return;
    }
    _isSeek = true;
    pause();
    pthread_mutex_lock(&_seekMutex);
    //单位是 微妙
    int64_t seek = i * 1000000;
    //seek到请求的时间 之前最近的关键帧
    // 只有从关键帧才能开始解码出完整图片
    av_seek_frame(avFormatContext, -1, seek, AVSEEK_FLAG_BACKWARD);
//    avformat_seek_file(formatContext, -1, INT64_MIN, seek, INT64_MAX, 0);
    // 音频、与视频队列中的数据 是不是就可以丢掉了？
    if (audioChannel) {
        //暂停队列
        audioChannel->stopWork();
        //可以清空缓存
//        avcodec_flush_buffers();
        audioChannel->clear();
        //启动队列
        audioChannel->startWork();
    }
    if (videoChannel) {
        videoChannel->stopWork();
        videoChannel->clear();
        videoChannel->startWork();
    }
    pthread_mutex_unlock(&_seekMutex);
    _isSeek = false;
    pause();
}

void DemoPlay::pause() {
    if (_isPrepare) {
        return;
    }
    if (_isPause) {
        if (audioChannel) {
            audioChannel->pause();
        }
        if (videoChannel) {
            videoChannel->pause();
        }

        _isPause = false;
        pthread_cond_broadcast(&_p_play_cond);
    } else {
        if (audioChannel) {
            audioChannel->pause();
        }
        if (videoChannel) {
            videoChannel->pause();
        }

        _isPause = true;
    }
}