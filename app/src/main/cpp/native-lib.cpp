#include <jni.h>
#include <string>
#include <android/log.h>

#define LOG_TAG "AudioRecordingApp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_audiorecordingapp_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Audio Recording App - Native Library Loaded";
    LOGI("Native library loaded successfully");
    return env->NewStringUTF(hello.c_str());
}

// JNI_OnLoad function to initialize the native library
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad called");
    return JNI_VERSION_1_6;
}
