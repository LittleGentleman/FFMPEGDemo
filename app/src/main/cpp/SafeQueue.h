//
// Created by gmm on 2020/6/28.
//

#ifndef FFMPEGDEMO_SAFEQUEUE_H
#define FFMPEGDEMO_SAFEQUEUE_H

#include "queue"
#include "pthread.h"

using namespace std;

//模板类
template<typename T>
class SafeQueue {
    typedef void (*ReleaseHandle)(T &);
    typedef void (*SyncHandle)(queue<T> &);

public:
    SafeQueue() {
        pthread_mutex_init(&mutex,0);
        pthread_cond_init(&cond,0);
    }

    ~SafeQueue(){
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mutex);
    }

    void enqueue(T newValue) {
        pthread_mutex_lock(&mutex);
        if (mEnable) {
            q.push(newValue);
            pthread_cond_signal(&cond);//发出通知，唤醒阻塞队列
        } else {
            releaseHandle(newValue);
        }
        pthread_mutex_unlock(&mutex);
    }

    int deQueue(T &value) {
        int ret = 0;
        pthread_mutex_lock(&mutex);
        //在多核处理器下 由于竞争可能虚假唤醒 包括jdk也说明了
        while (mEnable && q.empty()) {
            pthread_cond_wait(&cond,&mutex);//等待阻塞
        }
        if (!q.empty()) {
            value = q.front();
            q.pop();
            ret = 1;
        }
        pthread_mutex_unlock(&mutex);

        return ret;
    }

    void setEnable(bool enable) {
        pthread_mutex_lock(&mutex);
        mEnable = enable;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    int empty() {
        return q.empty();
    }

    int size() {
        return q.size();
    }

    void clear() {
        pthread_mutex_lock(&mutex);
        int size = q.size();
        for (int i = 0; i < size; ++i) {
            T value = q.front();
            releaseHandle(value);
            q.pop();
        }
        pthread_mutex_unlock(&mutex);
    }

    void sync() {
        pthread_mutex_lock(&mutex);
        syncHandle(q);
        pthread_mutex_unlock(&mutex);
    }

    void setReleaseHandle(ReleaseHandle r) {
        releaseHandle = r;
    }

    void setSyncHandle(SyncHandle s) {
        syncHandle = s;
    }

private:
    pthread_cond_t cond;
    pthread_mutex_t mutex;//互斥量 锁
    queue<T> q;
    bool mEnable;
    ReleaseHandle releaseHandle;
    SyncHandle syncHandle;
};

#endif //FFMPEGDEMO_SAFEQUEUE_H
