//
// Created by gmm on 2020/6/30.
//

#include "AudioChannel.h"
#include "Log.h"
extern "C" {
#include <libavutil/time.h>
}


void *audioDecode_t(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->decode();
    return 0;
}

void *audioPlay_t(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->_play();
    return 0;
}

AudioChannel::AudioChannel(int channelId, JavaCallHelper *helper, AVCodecContext *avCodecContext,
                           const AVRational &base) : BaseChannel(channelId, helper, avCodecContext,
                                                                 base) {
    //双声道：2
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    //每一个样本的大小 = 2个字节
    out_sampleSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    //采样率
    out_sampleRate = 44100;

    bufferCount = out_sampleRate * out_sampleSize * out_channels;
    //计算转换后数据的最大字节数
    buffer = static_cast<uint8_t *>(malloc(out_sampleRate * out_sampleSize * out_channels));
}

AudioChannel::~AudioChannel() {
    free(buffer);
    buffer = 0;
}

void AudioChannel::play() {

    //转换器上下文
    swrContext = swr_alloc_set_opts(0,
                                    AV_CH_LAYOUT_STEREO,//转换后的声道：双声道
                                    AV_SAMPLE_FMT_S16,//转换后的采样位：16位
                                    44100,//转换后的采样率
                                    avCodecContext->channel_layout,//音频源的声道
                                    avCodecContext->sample_fmt,//音频源的采样位
                                    avCodecContext->sample_rate,//音频源的采样率
                                    0,
                                    0);

    swr_init(swrContext);

    isPlaying = 1;
    setEnable(true);

    //解码
    pthread_create(&audioDecodeTask, 0, audioDecode_t, this);

    //播放
    pthread_create(&audioPlayTask, 0, audioPlay_t, this);

}

void AudioChannel::stop() {
    isPlaying = 0;
    helper = 0;
    setEnable(0);
    pthread_join(audioDecodeTask,0);
    pthread_join(audioPlayTask,0);

    _releaseOpenSL();
    if (swrContext) {
        swr_free(&swrContext);
        swrContext = 0;
    }
}

void AudioChannel::decode() {
    AVPacket *packet = 0;
    while (isPlaying) {
        int ret = pkt_queue.deQueue(packet);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }
        //向解码器发送待解码数据
        ret = avcodec_send_packet(avCodecContext, packet);
        releaseAvPacket(packet);
        if (ret < 0) {
            break;
        }
        //接收解码器解码后的数据
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avCodecContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {//失败
            break;
        }
        while (frame_queue.size() > 100 && isPlaying) {
            av_usleep(1000 * 10);
        }
        //解码成功，将已解码的数据添加到队列中
        frame_queue.enqueue(frame);
    }
}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf queue, void *pContext) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(pContext);
    int dataSize = audioChannel->_getData();
    if (dataSize > 0) {
        (*queue)->Enqueue(queue,audioChannel->buffer,dataSize);
    }
}

int AudioChannel::_getData() {
    int dataSize;
    AVFrame *frame = 0;
    while (isPlaying) {
        int ret = frame_queue.deQueue(frame);
        if (!isPlaying) {
            break;
        }
        if (!ret) {
            continue;
        }

        //转换
        int nb = swr_convert(swrContext, &buffer, bufferCount,
                             (const uint8_t **)(frame->data), frame->nb_samples);

        dataSize = nb * out_channels * out_sampleSize;
        //播放这一段声音的时间
        clock = frame->pts * av_q2d(time_base);
        break;
    }
    releaseAvFrame(frame);
    return dataSize;
}

//使用OpenSL ES 播放已解码的音频数据
void AudioChannel::_play() {
    /*
     * 1、创建引擎
     *
     */
    // 创建引擎engineObject
    SLresult result;
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 获取引擎接口engineInterface
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE,
                                           &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    /*
     * 2、创建混音器
     */
    //通过引擎接口创建混音器
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 初始化混音器outputMixObject
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    /*
     * 3、创建播放器
     */
    //创建buffer缓冲类型的队列作为数据定位器（获取播放数据） 2个缓冲区
    SLDataLocator_AndroidSimpleBufferQueue android_queue =
            {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    //pcm数据格式: pcm、声道数、采样率、采样位、容器大小、通道掩码(双声道)、字节序(小端)
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                            SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT |
                            SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
    //数据源 （数据获取器+格式）
    SLDataSource slDataSource = {&android_queue, &pcm};

    //设置混音器
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, NULL};

    //需要的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    //播放器相当于对混音器进行了一层封装，提供了额外的如：开始、停止等方法
    //  混音器来播放声音
    //创建播放器
    (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &slDataSource,
                                          &audioSnk, 1,
                                          ids, req);
    //初始化播放器
    (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);

    /*
     * 开始播放
     */
    //获得播放数据队列操作接口
//    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                    &bqPlayerBufferQueue);
    //设置回调（启动播放器后执行回调来获取数据并播放）
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

    //获取播放状态接口
//    SLPlayItf bqPlayerInterface = NULL;
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerInterface);
    // 设置播放状态
    (*bqPlayerInterface)->SetPlayState(bqPlayerInterface, SL_PLAYSTATE_PLAYING);

    //需要手动调用一次播放回调
    bqPlayerCallback(bqPlayerBufferQueue, this);
}

void AudioChannel::_releaseOpenSL() {
    LOGE("停止播放");
    //设置停止状态
    if (bqPlayerInterface) {
        (*bqPlayerInterface)->SetPlayState(bqPlayerInterface, SL_PLAYSTATE_STOPPED);
        bqPlayerInterface = 0;
    }
    //销毁播放器
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bqPlayerBufferQueue = 0;
    }
    //销毁混音器
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
    //销毁引擎
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineInterface = 0;
    }
}
