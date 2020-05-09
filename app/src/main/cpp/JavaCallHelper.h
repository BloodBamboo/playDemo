//
// Created by 12 on 2020/4/9.
//

#ifndef PLAYDEMO_JAVACALLHELPER_H
#define PLAYDEMO_JAVACALLHELPER_H


#include <jni.h>


class JavaCallHelper {
public:
    JavaCallHelper(JavaVM *jvm, JNIEnv *env, jobject instance);
    ~JavaCallHelper();
    //回调java
    void onError(int thread,int errorCode);
    void onPrepare(int thread);
    void onProgress(int thread, int progress);
    void onPlayEnd(int thread);
private:
    JavaVM* _jvm;
    JNIEnv* _env;
    jobject _instance;
    jmethodID _onErrorId;
    jmethodID _onPreparedId;
    jmethodID _onProgress;
    jmethodID _onPlayEnd;
};


#endif //PLAYDEMO_JAVACALLHELPER_H
