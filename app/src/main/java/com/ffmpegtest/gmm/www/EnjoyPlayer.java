package com.ffmpegtest.gmm.www;

import android.view.Surface;

/**
 * @author:gmm
 * @date:2020/6/27
 * @类说明:
 */
public class EnjoyPlayer {

    static {
        System.loadLibrary("native-lib");
    }

    //从C++层获得播放器句柄/指针
    private long nativeHandle;

    public EnjoyPlayer() {
        nativeHandle = nativeInit();
    }

    public void setDataSource(String path) {
        setDataSource(nativeHandle,path);
    }

    //获取媒体文件音视频信息，准备好解码器
    public void prepare() {
        prepare(nativeHandle);
    }

    public void setSurface(Surface surface) {
        setSurface(nativeHandle,surface);
    }

    public void start() {
        start(nativeHandle);
    }

    public void pause() {

    }

    public void stop() {
        stop(nativeHandle);
    }

    private native long nativeInit();

    private native void setDataSource(long nativeHandle,String path);

    private native void prepare(long nativeHandle);

    private native void start(long nativeHandle);

    private native void stop(long nativeHandle);

    private native void setSurface(long nativeHandle,Surface surface);

    private void onError(int code) {
        if (null != onErrorListener) {
            onErrorListener.onError(code);
        }
    }

    private void onPrepare() {
        if (null != onPrepareListener) {
            onPrepareListener.onPrepare();
        }
    }

    private void onProgress(int progress) {
        if (null != onProgressListener) {
            onProgressListener.onProgress(progress);
        }
    }

    public interface OnErrorListener{
        void onError(int code);
    }

    public interface OnPrepareListener{
        void onPrepare();
    }

    public interface OnProgressListener{
        void onProgress(int progress);
    }

    private OnErrorListener onErrorListener;

    private OnPrepareListener onPrepareListener;

    private OnProgressListener onProgressListener;

    public void setOnErrorListener(OnErrorListener onErrorListener) {
        this.onErrorListener = onErrorListener;
    }

    public void setOnPrepareListener(OnPrepareListener onPrepareListener) {
        this.onPrepareListener = onPrepareListener;
    }

    public void setOnProgressListener(OnProgressListener onProgressListener) {
        this.onProgressListener = onProgressListener;
    }
}
