#include <jni.h>
#include <android/log.h>
#include "emulator.hpp"

std::unique_ptr<Emulator> emulator;

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_Initialize(JNIEnv* env, jobject obj) {
    __android_log_print(ANDROID_LOG_INFO, "Panda3DS", "Initializing Alber Driver");
    emulator = std::make_unique<Emulator>();
    __android_log_print(ANDROID_LOG_INFO, "Panda3DS", "Done");
}