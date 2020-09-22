//
// Created by gmm on 2020/6/28.
//

#include "EnjoyPlayer.h"
#include <malloc.h>
#include <cstring>
#include "Log.h"

extern "C" {
#include <libavformat/avformat.h>
}

//子线程执行的方法
void *prepare_t(void *args) {
    EnjoyPlayer *player = static_cast<EnjoyPlayer *>(args);
//    avformat_open_input(0,player->path,0,0);//利用友元函数访问私有成员变量
    player->_prepare();
    return 0;
}

EnjoyPlayer::EnjoyPlayer(JavaCallHelper *_helper) : helper(_helper) {//初始化类表
    //初始化ffmpeg网络
    avformat_network_init();
    videoChannel = nullptr;
}

EnjoyPlayer::~EnjoyPlayer() {
    avformat_network_deinit();
    delete helper;
    helper = 0;
    if (path) {
        delete[] path;
        path = 0;
    }
}

void EnjoyPlayer::setDataSource(const char *path_) {

    //不能这样直接赋值指针，因为传过来的指针指向的内容后期可能会被回收，所以需要进行深拷贝
//    path = const_cast<char *>(path_);

    //C 的方式 分配内存 拷贝 字符串
    // + 1 : C/C++ 的字符串里是以'\0' 结尾，所以分配的内存需要+1
//    path = static_cast<char *>(malloc(strlen(path_) + 1));
//    memset(path,0,strlen(path_)+1);//初始化
//
//    memcpy(path,path_,strlen(path_));

    //C++ 的方式 分配内存 拷贝 字符串
    path = new char[strlen(path_) + 1];
    strcpy(path, path_);
}

void EnjoyPlayer::prepare() {
    //解析 耗时
    //开启线程
    /**
     * 参数1：线程的句柄、id
     * 参数2：线程的属性
     * 参数3：线程执行的方法
     * 参数4：参数3中方法的传参
     */
    pthread_create(&prepareTask, 0, prepare_t, this);
}

void EnjoyPlayer::_prepare() {
    avFormatContext = avformat_alloc_context();
    /*
     * ① 打开媒体文件
     */
    //参数3：输入文件的封装格式。avi / flv，传 0/NULL 表示自动检测格式
    //参数4：map集合，比如打开网络文件
    AVDictionary *opts;
    av_dict_set(&opts, "timeout", "3000000", 0);
    int ret = avformat_open_input(&avFormatContext, path, 0,
                                  &opts);//传指针的指针，是为了直接修改指针地址，而不是修改指针指向的内容
    if (ret != 0) {
        //失败
        LOGE("打开%s 失败，返回：%d 错误描述：%s", path, ret, av_err2str(ret));
        helper->onError(FFMPEG_CAN_NOR_OPEN_URL, THREAD_CHILD);//通知Java层（回调Java）
        return;
    }

    /*
     * ② 查找媒体流
     */
    ret = avformat_find_stream_info(avFormatContext, 0);// 小于0表示查找流失败
    if (ret < 0) {
        LOGE("查找媒体流 %s 失败，返回：%d 错误描述：%s", path, ret, av_err2str(ret));
        helper->onError(FFMPEG_CAN_NOT_FIND_STREAMS, THREAD_CHILD);
        return;
    }
    // 得到视频时长，单位是秒
    duration = avFormatContext->duration / AV_TIME_BASE;
    // nb_streams：这个媒体文件中有几个媒体流（视频流、音频流）
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        AVStream *avStream = avFormatContext->streams[i];
        // 解码信息
        AVCodecParameters *parameters = avStream->codecpar;
        // 查找解码器
        AVCodec *dec = avcodec_find_decoder(parameters->codec_id);
        if (!dec) {
            helper->onError(FFMPEG_FIND_DECODER_FAIL,THREAD_CHILD);
            return;
        }
        // 创建解码器上下文
        AVCodecContext *codecContext = avcodec_alloc_context3(dec);
        // 把解码信息赋值给了解码器上下文的各种成员
        ret = avcodec_parameters_to_context(codecContext,parameters);// 小于0表示失败
        if (ret < 0) {
            helper->onError(FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL,THREAD_CHILD);
            return;
        }
        // 打开解码器
        ret = avcodec_open2(codecContext,dec,0);// 等于0表示打开成功
        if (ret != 0) {
            helper->onError(FFMPEG_OPEN_DECODER_FAIL,THREAD_CHILD);
            return;
        }

        if (parameters->codec_type == AVMEDIA_TYPE_VIDEO) {//视频流
            //帧率
            double fps = av_q2d(avStream->avg_frame_rate);
            if (isnan(fps) || fps == 0) {
                fps = av_q2d(avStream->r_frame_rate);
            }
            if (isnan(fps) || fps == 0) {
                fps = av_q2d(av_guess_frame_rate(avFormatContext,avStream,0));
            }
            videoChannel = new VideoChannel(i,helper,codecContext,avStream->time_base,fps);
            videoChannel->setWindow(window);
        } else if (parameters->codec_type == AVMEDIA_TYPE_AUDIO) {//音频流
            audioChannel = new AudioChannel(i,helper,codecContext,avStream->time_base);
        }
    }

    // todo 还没处理音频
    // 如果媒体文件中没有视频
    if (!videoChannel && !audioChannel) {
        helper->onError(FFMPEG_NOMIDEA,THREAD_CHILD);
        return;
    }

    // 告诉Java准备好了，可以播放了
    helper->onPrepare(THREAD_CHILD);

}

void* start_t(void *args) {
    EnjoyPlayer *player = static_cast<EnjoyPlayer *>(args);
    player->_start();
    return 0;
}

void EnjoyPlayer::start() {
    //1.读取媒体源的数据
    //2.根据数据类型放入Audio/VideoChannel的队列中
    isPlaying = 1;
    if (videoChannel) {
        videoChannel->audioChannel = audioChannel;
        videoChannel->play();
    }
    if (audioChannel) {
        audioChannel->play();
    }
    pthread_create(&startTask,0,start_t,this);
}

void EnjoyPlayer::_start() {
    int ret;
    while (isPlaying) {
        AVPacket *packet = av_packet_alloc();//保存了解复用之后，解码之前的数据和关于这些数据的一些附加信息
        ret = av_read_frame(avFormatContext,packet);
        if (ret == 0) {
            if (videoChannel && packet->stream_index == videoChannel->channelId) {
                //视频
                videoChannel->pkt_queue.enqueue(packet);
            } else if (audioChannel && packet->stream_index == audioChannel->channelId) {
                //音频
                audioChannel->pkt_queue.enqueue(packet);
            } else {
                av_packet_free(&packet);
            }
        } else {
            av_packet_free(&packet);
            if (ret == AVERROR_EOF) {//end of file  表示文件读完了
                //读取完毕，不一定播放完毕
                if (videoChannel->pkt_queue.empty() && videoChannel->frame_queue.empty()
                        && audioChannel->pkt_queue.empty() && audioChannel->frame_queue.empty()) {//待解码队列和待播放队列同时为空，说明播放完毕
                    //播放完毕
                    break;
                }
            } else {
                LOGE("读取数据包失败，返回:%d 错误描述:%s", ret, av_err2str(ret));
                break;
            }
        }
    }

    isPlaying = 0;
    videoChannel->stop();
    audioChannel->stop();
}

void EnjoyPlayer::setWindow(ANativeWindow *window) {
    this->window = window;
    if (videoChannel) {
        videoChannel->setWindow(window);
    }
}

void EnjoyPlayer::stop() {
    isPlaying = 0;
    pthread_join(prepareTask,0);
    pthread_join(startTask,0);

    release();
}

void EnjoyPlayer::release() {
    if (audioChannel) {
        delete audioChannel;
        audioChannel = 0;
    }

    if (videoChannel) {
        delete videoChannel;
        videoChannel = 0;
    }

    if (avFormatContext) {
        avformat_close_input(&avFormatContext);
        avformat_free_context(avFormatContext);
        avFormatContext = 0;
    }
}
