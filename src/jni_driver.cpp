#include <jni.h>
#include <stdexcept>
#include <android/log.h>
#include <EGL/egl.h>
#include "renderer_gl/renderer_gl.hpp"
#include "emulator.hpp"

std::unique_ptr<Emulator> emulator = nullptr;
RendererGL* renderer = nullptr;
bool romLoaded = false;

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_Initialize(JNIEnv* env, jobject obj) {
    emulator = std::make_unique<Emulator>();
    if (emulator->getRendererType() != RendererType::OpenGL) {
        throw std::runtime_error("Renderer is not OpenGL");
    }
    renderer = static_cast<RendererGL*>(emulator->getRenderer());
    __android_log_print(ANDROID_LOG_INFO, "AlberDriver", "OpenGL ES Before %d.%d", GLVersion.major, GLVersion.minor);
    if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(eglGetProcAddress))) {
		throw std::runtime_error("OpenGL ES init failed");
	}
    __android_log_print(ANDROID_LOG_INFO, "AlberDriver", "OpenGL ES %d.%d", GLVersion.major, GLVersion.minor);
	emulator->initGraphicsContext(nullptr);
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_RunFrame(JNIEnv* env, jobject obj, jint fbo) {
    renderer->setFBO(fbo);
    renderer->resetStateManager();
    emulator->runFrame();
    
    emulator->getServiceManager().getHID().updateInputs(emulator->getTicks());
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_Finalize(JNIEnv* env, jobject obj) {
    emulator = nullptr;
    renderer = nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_panda3ds_pandroid_AlberDriver_HasRomLoaded(JNIEnv* env, jobject obj) {
    return romLoaded;
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_LoadRom(JNIEnv* env, jobject obj, jstring path) {
    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    __android_log_print(ANDROID_LOG_INFO, "AlberDriver", "Loading ROM %s", pathStr);
    romLoaded = emulator->loadROM(pathStr);
    env->ReleaseStringUTFChars(path, pathStr);
}



extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_TouchScreenDown(JNIEnv* env, jobject obj, jint x, jint y) {
    emulator->getServiceManager().getHID().setTouchScreenPress((u16)x, (u16)y);
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_TouchScreenUp(JNIEnv* env, jobject obj) {
    emulator->getServiceManager().getHID().releaseTouchScreen();
}


extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_KeyUp(JNIEnv* env, jobject obj, jint keyCode) {
    emulator->getServiceManager().getHID().releaseKey((u32)keyCode);
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_KeyDown(JNIEnv* env, jobject obj, jint keyCode) {
    emulator->getServiceManager().getHID().pressKey((u32)keyCode);
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_SetCirclepadAxis(JNIEnv* env, jobject obj, jint x, jint y) {
    emulator->getServiceManager().getHID().setCirclepadX((s16)x);
    emulator->getServiceManager().getHID().setCirclepadY((s16)y);
}


