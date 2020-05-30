//
// Created by 12 on 2020/4/9.
//

#include "VideoChannel.h"

//#define DEBUG

/**
 * 丢包 直到下一个关键帧
 * @param q
 */

void dropAvPacket(queue<AVPacket *> &q) {
    while (!q.empty()) {
        AVPacket *packet = q.front();
        //如果不属于 I 帧
        if (packet->flags != AV_PKT_FLAG_KEY) {
            BaseChannel::releaseAvPacket(&packet);
            q.pop();
        } else {
            break;
        }
    }
}

/**
 * 丢已经解码的图片
 * @param q
 */
void dropAvFrame(queue<AVFrame *> &q) {
    if (!q.empty()) {
        AVFrame *frame = q.front();
        BaseChannel::releaseAvFrame(&frame);
        q.pop();
    }
}

VideoChannel::VideoChannel(int id, AVCodecContext *avCodecContext, AVRational time_base, int fps,
                           JavaCallHelper *javaCallHelper, bool isRtmp)
        : BaseChannel(id, avCodecContext, time_base, javaCallHelper) {
    _isRtmp = isRtmp;
    _fps = fps;
    //  用于 设置一个 同步操作 队列的一个函数指针
//    packets.setSyncHandle(dropAvPacket);
    frames.setSyncHandle(dropAvFrame);
}

VideoChannel::~VideoChannel() {
//    stop();
}

void VideoChannel::setRenderFrameCallback(RenderFrameCallback callback) {
    _callback = callback;
}

void *decode_task(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->decode();
    return 0;
}

void *render_task(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->render();
    return 0;
}

void VideoChannel::play() {
    isPlaying = true;
    //设置为工作状态
    packets.setWork(1);
    frames.setWork(1);
    //1、解码
    pthread_create(&_pid_decode, 0, decode_task, this);
    //2、播放
    pthread_create(&_pid_render, 0, render_task, this);
}

//解码获取帧
void VideoChannel::decode() {
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
            LOGE("VideoChannel 取出失败");
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

//播放
void VideoChannel::render() {
    //目标： RGBA，把yuv转成rgba
    pthread_mutex_lock(&p_play);
    _swsContext = sws_getContext(
            avCodecContext->width, avCodecContext->height, avCodecContext->pix_fmt,
            avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, 0, 0, 0);
    //每个画面 刷新的间隔 单位：秒
    double frame_delays = 1.0 / _fps;
    AVFrame *frame = nullptr;
    //指针数组,用来把yuv转成rgba的数据缓存
    uint8_t *dst_data[4];
    int dst_linesize[4];
    av_image_alloc(dst_data, dst_linesize,
                   avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA, 1);
    while (isPlaying) {
        while (isPause) {
//            av_usleep(1000 * 10);
            pthread_cond_wait(&p_play_cond, &p_play);
        }
        int ret = frames.pop(frame);
        if (!ret) {
            LOGE("frames取出失败");
            continue;
        }
        if (!isPlaying) {
            break;
        }
        //src_linesize: 表示每一行存放的 字节长度
        sws_scale(_swsContext, reinterpret_cast<const uint8_t *const *>(frame->data),
                  frame->linesize, 0,
                  avCodecContext->height,
                  dst_data,
                  dst_linesize);

        //获得 当前这一个画面 播放的相对的时间
        //显示时间戳 什么时候显示这个frame
        if ((clock = frame->best_effort_timestamp) == AV_NOPTS_VALUE) {
            clock = 0;
        }
        clock = clock * av_q2d(time_base);

        /**
        *  seek需要注意的点：编码器中存在缓存
        *  100s 的图像,用户seek到第 50s 的位置
        *  音频是50s的音频，但是视频 你获得的是100s的视频
        */
        if (javaCallHelper && _audioChannel && !_isRtmp) {
            javaCallHelper->onProgress(THREAD_CHILD, clock);
        }

        //额外的间隔时间
        double extra_delay = frame->repeat_pict / (2 * _fps);
//        double extra_delay = frame->repeat_pict * (_fps * 0.5);
        // 真实需要的间隔时间
        double delays = extra_delay + frame_delays;
        if (!_audioChannel && _audioChannel->data) {
            //休眠，单位微妙 microseconds
//        //视频快了
//        av_usleep(frame_delays*1000000+x);
//        //视频慢了
//        av_usleep(frame_delays*1000000-x);
            av_usleep(delays * 1000000);
        } else {
            if (clock == 0 || _audioChannel->clock == 0) {
                av_usleep(delays * 1000000);
            } else {
                //比较音频与视频
                double audioClock = _audioChannel->clock;
                //间隔 音视频相差的间隔
                double diff = clock - audioClock;
                if (diff > 0) {
                    //大于0 表示视频比较快
#ifdef DEBUG
                    LOGE("视频快了：%lf", diff);
#endif
                    if (diff > 1) {
                        av_usleep((delays * 2) * 1000000);
                    } else {
                        av_usleep((delays + diff) * 1000000);
                    }
                } else if (diff < 0) {
                    //小于0 表示音频比较快
#ifdef DEBUG
                    LOGE("音频快了：%lf", diff);
#endif
                    // 视频包积压的太多了 （丢包）
                    //音频比视频快
                    //视频慢了 0.05s 已经比较明显了 (丢帧)
//                    if (diff > 1) {
//                        //一种可能： 快进了(因为解码器中有缓存数据，这样获得的avframe就和seek的匹配了)
//
//                    } else
                    if (diff >= 0.06) {
                        LOGE("视频包积压的太多了 （丢包）");
                        releaseAvFrame(&frame);
                        //执行同步操作 删除到最近的key frame
                        frames.sync();
                        continue;
                    } else {
                        //不休眠 加快速度赶上去
                    }
                }
            }
        }

        //回调出去进行播放
        if (isPlaying && !isPause) {
            _callback(dst_data[0], dst_linesize[0], avCodecContext->width, avCodecContext->height);
        }
        releaseAvFrame(&frame);
    }
    av_freep(&dst_data[0]);
    releaseAvFrame(&frame);
    isPlaying = false;
    isPause = false;
    sws_freeContext(_swsContext);
    _swsContext = nullptr;
    pthread_mutex_unlock(&p_play);
}

void VideoChannel::setAudioChannel(AudioChannel *pChannel) {
    _audioChannel = pChannel;
}

void VideoChannel::stop() {
    isPlaying = false;
    javaCallHelper = nullptr;
    frames.setWork(0);
    packets.setWork(0);
    pthread_join(_pid_decode, nullptr);
    pthread_join(_pid_render, nullptr);
    avcodec_flush_buffers(avCodecContext);
    _callback = nullptr;
    _audioChannel = nullptr;
    LOGE("VideoChannel::stop");
}


void VideoChannel::pause() {
    if (isPause) {
        isPause = false;
        pthread_cond_broadcast(&p_decode_cond);
        pthread_cond_broadcast(&p_play_cond);
    } else {
        isPause = true;
    }
}