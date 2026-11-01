#pragma once

#include <core/environment/Singleton.h>
#include <framework/platform/NativeTextInput.h>
#include <jni.h>

#include <mutex>
#include <thread>

struct android_app;
struct ALooper;

namespace sky {
    class AndroidNativeTextInput final : public Singleton<AndroidNativeTextInput> {
    public:
        bool Init(android_app *app);
        bool Show(const NativeTextInputOptions &options, NativeTextInputCallback callback);
        void Hide();
        void Shutdown();

    private:
        static jboolean JNICALL OnEvent(JNIEnv *env, jclass, jlong token, jint type, jstring text, jint error);
        static jboolean JNICALL IsActive(JNIEnv *, jclass, jlong token);
        void                    Wake();

        JavaVM         *vm       = nullptr;
        jobject         activity = nullptr;
        jmethodID       show     = nullptr;
        jmethodID       hide     = nullptr;
        std::thread::id owner;
        bool            accepting = false;
        std::mutex      looperMutex;
        ALooper        *looper = nullptr;
    };
} // namespace sky
