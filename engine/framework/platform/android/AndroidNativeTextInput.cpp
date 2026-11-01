#include "AndroidNativeTextInput.h"
#include "platform/NativeTextInputState.h"

#include <android/looper.h>
#include <core/logger/Logger.h>
#include <core/util/String.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <limits>
#include <utility>
#include <vector>

namespace sky {
    namespace {
        constexpr const char *TAG       = "AndroidNativeTextInput";
        constexpr int32_t     JNI_ERROR = -1;

        class ScopedEnv {
        public:
            explicit ScopedEnv(JavaVM *value) : vm(value)
            {
                if (vm == nullptr)
                    return;
                const int result = vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
                if (result == JNI_EDETACHED) {
                    attached = vm->AttachCurrentThread(&env, nullptr) == JNI_OK;
                    if (!attached)
                        env = nullptr;
                } else if (result != JNI_OK) {
                    env = nullptr;
                }
            }
            ~ScopedEnv()
            {
                if (attached && vm->DetachCurrentThread() != JNI_OK)
                    LOG_E(TAG, "Failed to detach native text input JNI thread");
            }
            JNIEnv *Get() const
            {
                return env;
            }

        private:
            JavaVM *vm;
            JNIEnv *env      = nullptr;
            bool    attached = false;
        };

        bool ClearException(JNIEnv *env)
        {
            if (!env->ExceptionCheck())
                return false;
            env->ExceptionClear();
            LOG_E(TAG, "Native text input JNI call failed");
            return true;
        }

        jstring JavaString(JNIEnv *env, const std::string &text)
        {
            const auto utf16 = Utf8ToUtf16U(text);
            if (utf16.size() > static_cast<size_t>(std::numeric_limits<jsize>::max()))
                return nullptr;
            std::vector<jchar> units(utf16.begin(), utf16.end());
            units.push_back(0);
            return env->NewString(units.data(), static_cast<jsize>(utf16.size()));
        }
    } // namespace

    bool AndroidNativeTextInput::Init(android_app *app)
    {
        if (activity != nullptr || app == nullptr || app->activity == nullptr || app->looper == nullptr)
            return false;
        ScopedEnv scope(app->activity->vm);
        auto     *env = scope.Get();
        if (env == nullptr)
            return false;
        const auto type = env->GetObjectClass(app->activity->javaGameActivity);
        if (ClearException(env) || type == nullptr)
            return false;
        const JNINativeMethod methods[] = {
            {const_cast<char *>("nativeTextInputEvent"), const_cast<char *>("(JILjava/lang/String;I)Z"), reinterpret_cast<void *>(OnEvent)},
            {const_cast<char *>("nativeTextInputActive"), const_cast<char *>("(J)Z"), reinterpret_cast<void *>(IsActive)}};
        const auto nextShow = env->GetMethodID(type, "showNativeTextInput", "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;IZZ)V");
        bool       valid    = !ClearException(env) && nextShow != nullptr;
        const auto nextHide = valid ? env->GetMethodID(type, "hideNativeTextInput", "(J)V") : nullptr;
        valid               = !ClearException(env) && valid && nextHide != nullptr;
        if (valid) {
            const int registered = env->RegisterNatives(type, methods, 2);
            valid                = !ClearException(env) && registered == JNI_OK;
        }
        const auto nextActivity = valid ? env->NewGlobalRef(app->activity->javaGameActivity) : nullptr;
        valid                   = !ClearException(env) && valid && nextActivity != nullptr;
        env->DeleteLocalRef(type);
        if (!valid)
            return false;
        vm        = app->activity->vm;
        activity  = nextActivity;
        show      = nextShow;
        hide      = nextHide;
        owner     = std::this_thread::get_id();
        accepting = true;
        std::lock_guard lock(looperMutex);
        looper = app->looper;
        ALooper_acquire(looper);
        return true;
    }

    bool AndroidNativeTextInput::Show(const NativeTextInputOptions &options, NativeTextInputCallback callback)
    {
        if (!accepting || activity == nullptr || owner != std::this_thread::get_id())
            return false;
        ScopedEnv scope(vm);
        auto     *env = scope.Get();
        if (env == nullptr)
            return false;
        auto      *state = NativeTextInputState::Get();
        const auto token = state->Begin(options, std::move(callback));
        if (token == 0)
            return false;
        const auto text        = JavaString(env, options.text);
        bool       failed      = ClearException(env) || text == nullptr;
        const auto placeholder = failed ? nullptr : JavaString(env, options.placeholder);
        failed                 = ClearException(env) || failed || placeholder == nullptr;
        const auto title       = failed ? nullptr : JavaString(env, options.title);
        failed                 = ClearException(env) || failed || title == nullptr;
        if (!failed) {
            env->CallVoidMethod(activity, show, static_cast<jlong>(token), text, placeholder, title, static_cast<jint>(options.maxLength),
                                static_cast<jboolean>(options.multiline), static_cast<jboolean>(options.password));
            failed = ClearException(env);
        }
        if (text)
            env->DeleteLocalRef(text);
        if (placeholder)
            env->DeleteLocalRef(placeholder);
        if (title)
            env->DeleteLocalRef(title);
        if (failed) {
            state->Fail(token, JNI_ERROR);
            Wake();
        }
        return true;
    }

    void AndroidNativeTextInput::Hide()
    {
        if (owner != std::this_thread::get_id())
            return;
        auto      *state = NativeTextInputState::Get();
        const auto token = state->ActiveToken();
        if (token == 0)
            return;
        ScopedEnv scope(vm);
        auto     *env = scope.Get();
        if (env != nullptr && activity != nullptr) {
            env->CallVoidMethod(activity, hide, static_cast<jlong>(token));
            if (ClearException(env))
                state->Fail(token, JNI_ERROR);
        }
        state->Cancel();
    }

    void AndroidNativeTextInput::Shutdown()
    {
        accepting = false;
        Hide();
        ScopedEnv scope(vm);
        auto     *env = scope.Get();
        if (activity != nullptr && env != nullptr)
            env->DeleteGlobalRef(activity);
        else if (activity != nullptr)
            LOG_E(TAG, "JNI unavailable while releasing text input Activity reference");
        activity = nullptr;
        vm       = nullptr;
        show = hide = nullptr;
        std::lock_guard lock(looperMutex);
        if (looper != nullptr)
            ALooper_release(std::exchange(looper, nullptr));
    }

    void AndroidNativeTextInput::Wake()
    {
        std::lock_guard lock(looperMutex);
        if (looper != nullptr)
            ALooper_wake(looper);
    }

    jboolean JNICALL AndroidNativeTextInput::OnEvent(JNIEnv *env, jclass, jlong token, jint type, jstring text, jint error)
    {
        if (token == 0 || type < static_cast<jint>(NativeTextInputEventType::CHANGED) || type >= static_cast<jint>(NativeTextInputEventType::END))
            return JNI_FALSE;
        NativeTextInputEvent event{static_cast<NativeTextInputEventType>(type), {}, error};
        bool                 conversionFailed = text == nullptr;
        if (text != nullptr) {
            const auto  length = env->GetStringLength(text);
            const auto *units  = env->GetStringChars(text, nullptr);
            if (ClearException(env) || units == nullptr) {
                conversionFailed = true;
            } else {
                std::u16string utf16;
                utf16.reserve(static_cast<size_t>(length));
                for (jsize index = 0; index < length; ++index)
                    utf16.push_back(static_cast<char16_t>(units[index]));
                env->ReleaseStringChars(text, units);
                event.text = Utf16ToUtf8(utf16);
            }
        }
        auto      *state = NativeTextInputState::Get();
        const bool posted =
            conversionFailed ? state->Fail(static_cast<uint64_t>(token), JNI_ERROR) : state->Post(static_cast<uint64_t>(token), std::move(event));
        if (posted)
            Get()->Wake();
        return posted && !conversionFailed;
    }

    jboolean JNICALL AndroidNativeTextInput::IsActive(JNIEnv *, jclass, jlong token)
    {
        return token != 0 && NativeTextInputState::Get()->ActiveToken() == static_cast<uint64_t>(token);
    }
} // namespace sky
