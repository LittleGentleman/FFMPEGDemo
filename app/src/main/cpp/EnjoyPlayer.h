//
// Created by gmm on 2020/6/28.
//

#ifndef FFMPEGDEMO_ENJOYPLAYER_H
#define FFMPEGDEMO_ENJOYPLAYER_H

#include <pthread.h>
#include "JavaCallHelper.h"
#include "VideoChannel.h"
#include "AudioChannel.h"
#include <android/native_window_jni.h>

extern "C" {
#include <libavformat/avformat.h>
};

class EnjoyPlayer {
    //友元函数 可以访问私有成员属性
    friend void * prepare_t(void *args);

    friend void* start_t(void *args);

public:
    EnjoyPlayer(JavaCallHelper *_helper);

    ~EnjoyPlayer();

    void setDataSource(const char *path);

    void prepare();

    void start();

    void setWindow(ANativeWindow *surface);

    void stop();

private:
    void _prepare();
    void _start();

private:
    char *path;
    pthread_t prepareTask;//线程句柄
    JavaCallHelper *helper;
    int64_t duration;//视频总时长
    VideoChannel *videoChannel;
    AudioChannel *audioChannel;

    pthread_t startTask;
    bool isPlaying;
    AVFormatContext *avFormatContext;

    ANativeWindow *window = 0;//一定要初始化
    void release();
};


#endif //FFMPEGDEMO_ENJOYPLAYER_H
