//
// Created by gmm on 2020/6/28.
//

#ifndef FFMPEGDEMO_BASECHANNEL_H
#define FFMPEGDEMO_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
};

#include "JavaCallHelper.h"
#include "SafeQueue.h"

class BaseChannel {
public:
    BaseChannel(int channelId,JavaCallHelper *helper,AVCodecContext *avCodecContext,
    AVRational base):channelId(channelId),
                    helper(helper),
                    avCodecContext(avCodecContext),
                    time_base(base){
        pkt_queue.setReleaseHandle(releaseAvPacket);
        frame_queue.setReleaseHandle(releaseAvFrame);

    }

    virtual ~BaseChannel() {
        if (avCodecContext) {
            avcodec_close(avCodecContext);
            avcodec_free_context(&avCodecContext);
            avCodecContext = nullptr;
        }
        pkt_queue.clear();
        frame_queue.clear();
    }

    //纯虚函数 相当于Java中的抽象函数
    virtual void play() = 0;

    virtual void stop() = 0;

    virtual void decode() = 0;

    void setEnable(bool enable) {
        pkt_queue.setEnable(enable);
        frame_queue.setEnable(enable);
    }

    static void releaseAvPacket(AVPacket *&packet) {
        if (packet) {
            av_packet_free(&packet);
            packet = nullptr;
        }
    }

    static void releaseAvFrame(AVFrame *&frame) {
        if (frame) {
            av_frame_free(&frame);
            frame = nullptr;
        }
    }

public:
    int channelId;
    JavaCallHelper *helper;
    AVCodecContext *avCodecContext;
    AVRational time_base;

    SafeQueue<AVPacket *> pkt_queue;//待解码队列
    SafeQueue<AVFrame *> frame_queue;//待播放队列
    bool isPlaying = false;

    double clock = 0;
};

#endif //FFMPEGDEMO_BASECHANNEL_H
