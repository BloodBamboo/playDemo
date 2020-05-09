#include <jni.h>
#include <string>
#include "DemoPlay.h"
#include <android/log.h>
#include <android/native_window_jni.h>
#include "JavaCallHelper.h"

extern "C" {
#include "libavutil/avutil.h"
}

/**
 * 不同版本的ndk对于不同机型的内存查看可能存在不同结果，换个版本可能就可以就那些profiler的查看使用，
 * 也有肯能是ndk版本21使用clang编译导致手机使用profiler，手机可能android版本比较高，低版本ndk不能进行profiler
 */

DemoPlay *demoPlay = nullptr;
JavaVM *javaVm = nullptr;
ANativeWindow *window = nullptr;
JavaCallHelper *javaCallHelper = nullptr;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int JNI_OnLoad(JavaVM *vm, void *reserved) {
    javaVm = vm;
    return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM *vm, void *reserved) {
    DELETE(demoPlay);
    DELETE(javaCallHelper);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_playdemo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    LOGE("c++ ffmepg begin %s", av_version_info());
    return env->NewStringUTF(av_version_info());
}

//绘制图形到手机上
void render(uint8_t *data, int lineszie, int w, int h) {
    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex);
        return;
    }

    //设置窗口属性
    ANativeWindow_setBuffersGeometry(window, w,
                                     h,
                                     WINDOW_FORMAT_RGBA_8888);

    ANativeWindow_Buffer window_buffer;

    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        ANativeWindow_release(window);
        window = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }
    //填充rgb数据给dst_data
    uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
    // stride：一行多少个数据（RGBA） *4
    int dst_linesize = window_buffer.stride * 4;
    //一行一行的拷贝
    for (int i = 0; i < window_buffer.height; ++i) {
        //memcpy(dst_data , data, dst_linesize);
        memcpy(dst_data + i * dst_linesize, data + i * lineszie, dst_linesize);
    }

    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_playdemo_Play_native_1prepare(JNIEnv *env, jobject thiz, jstring data_source) {
    //获取视频路径地址
    const char *source = env->GetStringUTFChars(data_source, nullptr);
    LOGE("data_source_path %s", source);
    //创建DemoPlay实例
    demoPlay = nullptr;
    DELETE(javaCallHelper);
    javaCallHelper = new JavaCallHelper(javaVm, env, thiz);
    demoPlay = new DemoPlay(javaCallHelper, source);
    demoPlay->setRenderFrameCallback(render);
    demoPlay->prepare();
    env->ReleaseStringUTFChars(data_source, source);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_playdemo_Play_native_1start(JNIEnv *env, jobject thiz) {
    demoPlay->start();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_playdemo_Play_native_1setSurface(JNIEnv *env, jobject thiz, jobject surface) {
    pthread_mutex_lock(&mutex);
    if (window) {
        //把老的释放
        ANativeWindow_release(window);
        window = nullptr;
    }
    window = ANativeWindow_fromSurface(env, surface);
    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_playdemo_Play_native_1stop(JNIEnv *env, jobject thiz) {
    if (demoPlay) {
        demoPlay->stop();
        demoPlay = nullptr;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_playdemo_Play_native_1release(JNIEnv *env, jobject thiz) {
    pthread_mutex_lock(&mutex);
    if (window) {
        //把老的释放
        ANativeWindow_release(window);
        window = 0;
    }
    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_playdemo_Play_native_1getDuration(JNIEnv *env, jobject thiz) {
    if (demoPlay) {
        int temp = demoPlay->getDuration();
        LOGE("总时长  %d", temp);
        return temp;
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_playdemo_Play_native_1seek(JNIEnv *env, jobject thiz, jint progress) {
    if (demoPlay) {
        LOGE("native_1seek  %d", progress);
        demoPlay->seek(progress);
    }
}extern "C"
JNIEXPORT void JNICALL
Java_com_example_playdemo_Play_native_1pause(JNIEnv *env, jobject thiz) {
    if (demoPlay) {
        demoPlay->pause();
    }
}