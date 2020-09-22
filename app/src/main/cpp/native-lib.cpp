#include <jni.h>
#include <string>
#include "EnjoyPlayer.h"
#include "JavaCallHelper.h"
#include "Log.h"
JavaVM *javaVM = 0;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm,void *reserved) {
    javaVM = vm;
    return JNI_VERSION_1_4;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_ffmpegtest_gmm_www_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_ffmpegtest_gmm_www_EnjoyPlayer_nativeInit(JNIEnv *env, jobject instance) {//instance：调用本地方法init的Java对象，当前为Java层的EnjoyPlayer对象

    EnjoyPlayer *player = new EnjoyPlayer(new JavaCallHelper(javaVM,env,instance));

    //reinterpret_cast 转换成其他类型
    return reinterpret_cast<jlong>(player);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpegtest_gmm_www_EnjoyPlayer_setDataSource(JNIEnv *env,
                                                      jobject instance,
                                                      jlong nativeHandle,jstring path_) {

    const char *path = env->GetStringUTFChars(path_, 0);
    EnjoyPlayer *player = reinterpret_cast<EnjoyPlayer *>(nativeHandle);
    player->setDataSource(path);

    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpegtest_gmm_www_EnjoyPlayer_prepare(JNIEnv *env, jobject instance,
                                                   jlong nativeHandle) {

    EnjoyPlayer *player = reinterpret_cast<EnjoyPlayer *>(nativeHandle);
    player->prepare();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpegtest_gmm_www_EnjoyPlayer_start(JNIEnv *env, jobject instance,
                                                 jlong nativeHandle) {

    EnjoyPlayer *player = reinterpret_cast<EnjoyPlayer *>(nativeHandle);
    player->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpegtest_gmm_www_EnjoyPlayer_setSurface(JNIEnv *env,
                                                                             jobject instance,
                                                                             jlong nativeHandle,
                                                                             jobject surface) {

    EnjoyPlayer *player = reinterpret_cast<EnjoyPlayer *>(nativeHandle);
    ANativeWindow *window = ANativeWindow_fromSurface(env,surface);
    player->setWindow(window);

}

extern "C"
JNIEXPORT void JNICALL
Java_com_ffmpegtest_gmm_www_EnjoyPlayer_stop__J(JNIEnv *env, jobject instance, jlong nativeHandle) {

    // TODO
    EnjoyPlayer *player = reinterpret_cast<EnjoyPlayer *>(nativeHandle);
    player->stop();
    delete player;

}