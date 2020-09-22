//
// Created by gmm on 2020/6/28.
//

#include "VideoChannel.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/rational.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

#import "Log.h"

#define AV_SYNC_THRESHOLD_MIN 0.04
#define AV_SYNC_THRESHOLD_MAX 0.1

VideoChannel::VideoChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext,
                           const AVRational &base, double fps) : BaseChannel(channelId, helper,
                                                                          avCodecContext, base),
                                                              fps(fps) {

    pthread_mutex_init(&surfaceMutex, 0);
}

VideoChannel::~VideoChannel() {
    pthread_mutex_destroy(&surfaceMutex);
}

void *videoDecode_t(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->decode();
    return 0;
}

void *videoPlay_t(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->_play();
    return 0;
}

void VideoChannel::_play() {//显示
    //将视频YUN格式转换为RGB格式（因为ANativeWindow只能显示RGB格式图像）
    //缩放、格式转换
    SwsContext *swsContext = sws_getContext(avCodecContext->width,
                                            avCodecContext->height,
                                            avCodecContext->pix_fmt,
                                            avCodecContext->width,
                                            avCodecContext->height,
                                            AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR,
                                            0, 0, 0);

    uint8_t *data[4];
    int linesize[4];
    av_image_alloc(data, linesize, avCodecContext->width, avCodecContext->height, AV_PIX_FMT_RGBA,
                   1);

    AVFrame *frame = 0;
    double frame_delay = 1.0 / fps;//单位：秒
    int ret;
    while (isPlaying) {
        //阻塞方法
        ret = frame_queue.deQueue(frame);//从队列中取已解码的数据
        //阻塞完了之后，可能用户已经停止播放（阻塞过程中，用户可能停止了播放，isPlaying为false,当队列停止阻塞正常返回后,那么就不需要再显示图像）
        if (!isPlaying) {
            break;
        }

        if (!ret) {//取失败
            continue;
        }

        //让视频流畅播放
        //官方要求的额外延迟时间
        double extra_delay = frame->repeat_pict / (2 * fps);
        LOGE("frame->repeat_pict=%lf",frame->repeat_pict);
        //总延时
        double delay = frame_delay + extra_delay;


        if (audioChannel) {
            // best_effort_timestamp = pts  显示时间戳
            clock = frame->best_effort_timestamp * av_q2d(time_base);
            double diff = clock - audioChannel->clock;

            /*
             * 1、delay < 0.04 ，同步阈值就是0.04
             * 2、delay > 0.1，同步阈值就是0.1
             * 3、0.04 < delay < 0.1 ，同步阈值就是delay
             */
            // 根据每秒视频需要播放的图像数，确定音视频的时间差允许范围
            //设置一个时间差允许范围
            double sync = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
            LOGE("sync:%lf",sync);
            if (diff <= -sync) {//视频落后音频太多了，需要同步
                //减小delay时间，加快图像显示，追赶音频
                delay = FFMAX(0,delay + diff);
            } else if (diff > sync) {//音频落后视频
                //增加delay时间，减慢图像显示，等待音频
                delay = delay + diff;
            }

            LOGE("Video:%lf Audio:%lf delay:%lf A-V=%lf",clock,audioChannel->clock,delay,-diff);
        }



        //每显示一帧，延迟这些时间
        av_usleep(delay * 1000000);//转化成微妙



        //todo 2、指针数组，比如RGBA，每一个维度的数据就是一个指针，那么RGBA需要4个指针，所以就是4个元素的数组，数组的元素就是指针，指针数组
        //3、每一行数据的个数
        //4、offset 偏移量
        //5、要转换图像的高
        //6、7、 接收转换结果
        sws_scale(swsContext,
                  frame->data,
                  frame->linesize,
                  0,
                  frame->height,
                  data, linesize);

        //画画
        onDraw(data, linesize, avCodecContext->width, avCodecContext->height);
        releaseAvFrame(frame);
    }

    av_free(&data[0]);
    isPlaying = 0;
    releaseAvFrame(frame);
    sws_freeContext(swsContext);
}

void VideoChannel::onDraw(uint8_t **data, int *linesize, int width, int height) {
    pthread_mutex_lock(&surfaceMutex);
    if (!window) {
        pthread_mutex_unlock(&surfaceMutex);
        return;
    }
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);

    ANativeWindow_Buffer buffer;
    int ret = ANativeWindow_lock(window, &buffer, 0);
    if (ret != 0) {//window 和 buffer绑定失败
        ANativeWindow_release(window);
        window = 0;
        pthread_mutex_unlock(&surfaceMutex);
        return;
    }
    //把视频数据刷到buffer中
    uint8_t *dstData = static_cast<uint8_t *>(buffer.bits);
    int dstSize = buffer.stride * 4;//因为ANativeWindow需要字节补齐（提高解析效率），stride的值是>=视频的width的
    //视频图像的rgba数据
    uint8_t *srcData = data[0];
    int srcSize = linesize[0];

    //一行一行拷贝
    for (int i = 0; i < buffer.height; ++i) {
        memcpy(dstData + i * dstSize, srcData + i * srcSize, srcSize);
    }

    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&surfaceMutex);
}

void VideoChannel::play() {
    isPlaying = 1;
    setEnable(true);//开启队列的工作
    //解码
    pthread_create(&videoDecodeTask, 0, videoDecode_t, this);
    //播放
    pthread_create(&videoPlayTask, 0, videoPlay_t, this);

}

void VideoChannel::decode() {
    AVPacket *avPacket = 0;
    while (isPlaying) {
        //阻塞
        //1.能够取到数据
        //2.停止播放
        int ret = pkt_queue.deQueue(avPacket);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        // 向解码器发送待解码数据
        ret = avcodec_send_packet(avCodecContext, avPacket);
        releaseAvPacket(avPacket);//释放AVPacket
        if (ret < 0) {
            //发送失败
            break;
        }

        //从解码器取出解码好的数据
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, frame);
        if (ret == AVERROR(EAGAIN)) {//数据不够
            continue;
        } else if (ret < 0) {
            //失败
            break;
        }
        //todo 这里还有音频的，更好的实现是需要时再解码，只需要一个线程与一个待解码队列
        while (frame_queue.size() > fps * 10 && isPlaying) {
            av_usleep(1000 * 10);
        }
        frame_queue.enqueue(frame);//将解码后的数据添加到待播发队列中
    }
    releaseAvPacket(avPacket);
}

void VideoChannel::setWindow(ANativeWindow *window) {//因为setWindow(设置播放窗口)是在主线程，但是播放是在子线程，为了线程安全需要加锁
    pthread_mutex_lock(&surfaceMutex);
    if (this->window) {//不为空，先释放
        ANativeWindow_release(this->window);
    }
    this->window = window;
    pthread_mutex_unlock(&surfaceMutex);
}

void VideoChannel::stop() {
    isPlaying = 0;
    helper = 0;
    setEnable(0);
    pthread_join(videoDecodeTask,0);
    pthread_join(videoPlayTask,0);
    if (window) {
        ANativeWindow_release(window);
        window = 0;
    }
}


