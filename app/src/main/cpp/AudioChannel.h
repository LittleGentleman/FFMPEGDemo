//
// Created by gmm on 2020/6/30.
//

#ifndef FFMPEGDEMO_AUDIOCHANNEL_H
#define FFMPEGDEMO_AUDIOCHANNEL_H


#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "BaseChannel.h"

extern "C" {
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
};

class AudioChannel : public BaseChannel {

    friend void* audioPlay_t(void *args);
    friend void bqPlayerCallback(SLAndroidSimpleBufferQueueItf queue, void *pContext);

public:
    AudioChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext,
                 const AVRational &base);

    virtual ~AudioChannel();

public:
    virtual void play();

    virtual void stop();

    virtual void decode();

private:
    void _play();

    int _getData();

private:
    pthread_t audioDecodeTask;
    pthread_t audioPlayTask;
    SwrContext *swrContext = 0;
    int out_channels;
    int out_sampleSize;
    int out_sampleRate;
    uint8_t  *buffer;
    int bufferCount;

    SLObjectItf engineObject = NULL;
    SLEngineItf engineInterface = NULL;
    SLObjectItf outputMixObject = NULL;
    SLObjectItf bqPlayerObject = NULL;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
    SLPlayItf bqPlayerInterface = NULL;

    void _releaseOpenSL();
};


#endif //FFMPEGDEMO_AUDIOCHANNEL_H
