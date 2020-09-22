//
// Created by gmm on 2020/6/28.
// C++ 通过 JNI 调 Java方法
//

#include "JavaCallHelper.h"

JavaCallHelper::JavaCallHelper(JavaVM *_javaVM, JNIEnv *_env, jobject &_jobj):javaVM(_javaVM),env(_env) {
    jobj = env->NewGlobalRef(_jobj);
    jclass jclazz = env->GetObjectClass(jobj);

    jmid_error = env->GetMethodID(jclazz,"onError","(I)V");
    jmid_prepare = env->GetMethodID(jclazz,"onPrepare","()V");
    jmid_progress = env->GetMethodID(jclazz,"onProgress","(I)V");
}

JavaCallHelper::~JavaCallHelper() {
    env->DeleteGlobalRef(jobj);
    jobj = nullptr;
}

void JavaCallHelper::onError(int code, int thread) {//调用Java对应的onError方法
    if (thread == THREAD_CHILD) {
        //子线程
        //JNIEnv 是与线程绑定的
        JNIEnv *jniEnv;//初始化一个新的JNIEnv，用于子线程
        if (javaVM->AttachCurrentThread(&jniEnv,0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(jobj,jmid_error,code);
        javaVM->DetachCurrentThread();
    } else {
        //这个env是主线程的
        env->CallVoidMethod(jobj,jmid_error,code);
    }
}

void JavaCallHelper::onPrepare(int thread) {
    if (thread == THREAD_CHILD) {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv,0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(jobj,jmid_prepare);
        javaVM->DetachCurrentThread();
    } else {
        env->CallVoidMethod(jobj,jmid_prepare);
    }
}

void JavaCallHelper::onProgress(int progress, int thread) {
    if (thread == THREAD_CHILD) {
        JNIEnv *jniEnv;
        if (javaVM->AttachCurrentThread(&jniEnv,0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(jobj,jmid_progress,progress);
    } else {
        env->CallVoidMethod(jobj,jmid_progress,progress);
    }
}