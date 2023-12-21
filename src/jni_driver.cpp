#include <EGL/egl.h>
#include <android/log.h>
#include <jni.h>

#include <stdexcept>

#include "emulator.hpp"
#include "renderer_gl/renderer_gl.hpp"
#include "services/hid.hpp"

#include "jni_driver.hpp"

std::unique_ptr<Emulator> emulator = nullptr;
HIDService* hidService = nullptr;
RendererGL* renderer = nullptr;
bool romLoaded = false;
JavaVM* jvm = nullptr;
const char* alberClass = "com/panda3ds/pandroid/AlberDriver";

#define AlberFunction(type, name) JNIEXPORT type JNICALL Java_com_panda3ds_pandroid_AlberDriver_##name

void throwException(JNIEnv* env, const char* message) {
	jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
	env->ThrowNew(exceptionClass, message);
}

JNIEnv* jniEnv(){
    JNIEnv* env;
    auto status = jvm->GetEnv((void **)&env, JNI_VERSION_1_6);
    if(status == JNI_EDETACHED){
        jvm->AttachCurrentThread(&env, nullptr);
    } else if(status != JNI_OK){
        throw std::runtime_error("Failed to obtain JNIEnv from JVM!!");
    }
    return env;
}


void Pandroid::onSmdhLoaded(const std::vector<u8> &smdh){
    JNIEnv* env = jniEnv();
    int size = smdh.size();
    
    jbyteArray result = env->NewByteArray(size);
    
    jbyte buffer[size];
    
    for(int i = 0; i < size; i++){
        buffer[i] = (jbyte) smdh[i];
    }
    env->SetByteArrayRegion(result, 0, size, buffer);

    
    auto clazz = env->FindClass(alberClass);
    auto method = env->GetStaticMethodID(clazz, "OnSmdhLoaded", "([B)V");

    env->CallStaticVoidMethod(clazz, method, result);

    env->DeleteLocalRef(result);
}


extern "C" {

AlberFunction(void, Setup)(JNIEnv* env, jobject obj) {
    env->GetJavaVM(&jvm);
}

AlberFunction(void, Initialize)(JNIEnv* env, jobject obj) {
	emulator = std::make_unique<Emulator>();

	if (emulator->getRendererType() != RendererType::OpenGL) {
		return throwException(env, "Renderer type is not OpenGL");
	}

	renderer = static_cast<RendererGL*>(emulator->getRenderer());
	hidService = &emulator->getServiceManager().getHID();

	if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(eglGetProcAddress))) {
		return throwException(env, "Failed to load OpenGL ES 2.0");
	}

	__android_log_print(ANDROID_LOG_INFO, "AlberDriver", "OpenGL ES %d.%d", GLVersion.major, GLVersion.minor);
	emulator->initGraphicsContext(nullptr);
}

AlberFunction(void, RunFrame)(JNIEnv* env, jobject obj, jint fbo) {
	renderer->setFBO(fbo);
	// TODO: don't reset entire state manager
	renderer->resetStateManager();
	emulator->runFrame();

	hidService->updateInputs(emulator->getTicks());
}

AlberFunction(void, Finalize)(JNIEnv* env, jobject obj) {
	emulator = nullptr;
	hidService = nullptr;
	renderer = nullptr;
}

AlberFunction(jboolean, HasRomLoaded)(JNIEnv* env, jobject obj) { return romLoaded; }

AlberFunction(void, LoadRom)(JNIEnv* env, jobject obj, jstring path) {
	const char* pathStr = env->GetStringUTFChars(path, nullptr);
	romLoaded = emulator->loadROM(pathStr);
	env->ReleaseStringUTFChars(path, pathStr);
}

AlberFunction(void, TouchScreenDown)(JNIEnv* env, jobject obj, jint x, jint y) { hidService->setTouchScreenPress((u16)x, (u16)y); }
AlberFunction(void, TouchScreenUp)(JNIEnv* env, jobject obj) { hidService->releaseTouchScreen(); }
AlberFunction(void, KeyUp)(JNIEnv* env, jobject obj, jint keyCode) { hidService->releaseKey((u32)keyCode); }
AlberFunction(void, KeyDown)(JNIEnv* env, jobject obj, jint keyCode) { hidService->pressKey((u32)keyCode); }

AlberFunction(void, SetCirclepadAxis)(JNIEnv* env, jobject obj, jint x, jint y) {
	hidService->setCirclepadX((s16)x);
	hidService->setCirclepadY((s16)y);
}
}

#undef AlberFunction
