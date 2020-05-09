//
// Created by 12 on 2020/4/9.
//

#include "JavaCallHelper.h"
#include "macro.h"

JavaCallHelper::JavaCallHelper(JavaVM *jvm, JNIEnv *env, jobject instance) {
    _jvm = jvm;
    //如果在主线程 回调
    _env = env;
    // 一旦涉及到jobject 跨方法 跨线程 就需要创建全局引用
    _instance = env->NewGlobalRef(instance);

    jclass clazz = _env->GetObjectClass(_instance);
    _onPreparedId = _env->GetMethodID(clazz, "onPrepared", "()V");
    _onErrorId = _env->GetMethodID(clazz, "onError", "(I)V");
    _onProgress = _env->GetMethodID(clazz, "onProgress", "(I)V");
    _onPlayEnd = _env->GetMethodID(clazz, "onPlayEnd", "()V");
}

JavaCallHelper::~JavaCallHelper() {
    _env->DeleteGlobalRef(_instance);
}

void JavaCallHelper::onError(int thread, int errorCode) {
    //主线程
    if (thread == THREAD_MAIN) {
        _env->CallVoidMethod(_instance, _onErrorId);
    } else {
        //子线程
        JNIEnv *env = nullptr;
        //获得属于我这一个线程的jnienv
        _jvm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(_instance, _onErrorId);
        //解除绑定
        _jvm->DetachCurrentThread();
    }
}

void JavaCallHelper::onPrepare(int thread) {
    if (thread == THREAD_MAIN) {
        _env->CallVoidMethod(_instance, _onPreparedId);
    } else {
        JNIEnv *env = nullptr;
        _jvm->AttachCurrentThread(&env, nullptr);
        env->CallVoidMethod(_instance, _onPreparedId);
        _jvm->DetachCurrentThread();
    }
}

void JavaCallHelper::onProgress(int thread, int progress) {
    if (thread == THREAD_CHILD) {
        JNIEnv *jniEnv;
        if (_jvm->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(_instance, _onProgress, progress);
        _jvm->DetachCurrentThread();
    } else {
        _env->CallVoidMethod(_instance, _onProgress, progress);
    }
}

void JavaCallHelper::onPlayEnd(int thread) {
    if (thread == THREAD_CHILD) {
        JNIEnv *jniEnv;
        if (_jvm->AttachCurrentThread(&jniEnv, 0) != JNI_OK) {
            return;
        }
        jniEnv->CallVoidMethod(_instance, _onPlayEnd);
        _jvm->DetachCurrentThread();
    } else {
        _env->CallVoidMethod(_instance, _onPlayEnd);
    }
}