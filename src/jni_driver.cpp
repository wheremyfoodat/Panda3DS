#include <jni.h>
#include <stdexcept>
#include <android/log.h>
#include <EGL/egl.h>
#include "renderer_gl/renderer_gl.hpp"
#include "emulator.hpp"

std::unique_ptr<Emulator> emulator = nullptr;
RendererGL* renderer = nullptr;

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
    // girlboss it for now :3
    emulator->loadROM("/data/data/com.panda3ds.pandroid/files/rom.3ds");
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_RunFrame(JNIEnv* env, jobject obj, jint fbo) {
    renderer->setFBO(fbo);
    renderer->resetStateManager();
    emulator->runFrame();
}

extern "C" JNIEXPORT void JNICALL Java_com_panda3ds_pandroid_AlberDriver_Finalize(JNIEnv* env, jobject obj) {
    emulator = nullptr;
    renderer = nullptr;
}