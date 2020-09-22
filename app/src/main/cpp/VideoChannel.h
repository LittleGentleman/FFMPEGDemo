//
// Created by gmm on 2020/6/28.
//

#ifndef FFMPEGDEMO_VIDEOCHANNEL_H
#define FFMPEGDEMO_VIDEOCHANNEL_H


#include <android/native_window.h>
#include "BaseChannel.h"
#include "AudioChannel.h"

class VideoChannel : public BaseChannel {
    friend void *videoPlay_t(void *args);//友元函数，可以访问私有成员变脸和私有成员函数

public:
    VideoChannel(int channelId,JavaCallHelper *helper,AVCodecContext *avCodecContext,
                 const AVRational &base, double fps);

    virtual ~VideoChannel();

    void setWindow(ANativeWindow *window);

public:
    virtual void play();

    virtual void stop();

    virtual void decode();

private:
    void _play();

private:
    double fps;
    pthread_mutex_t surfaceMutex;
    pthread_t videoDecodeTask;
    bool isPlaying;
    pthread_t videoPlayTask;
    ANativeWindow *window = 0;

public:
    AudioChannel *audioChannel;


    void onDraw(uint8_t *data[4], int linesize[4], int width, int height);
};



#endif //FFMPEGDEMO_VIDEOCHANNEL_H
