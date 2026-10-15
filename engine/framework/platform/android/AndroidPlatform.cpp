//
// Created by Zach Lee on 2022/9/26.
//

#include "AndroidPlatform.h"
#include "AndroidPerfManager.h"
#include "AndroidWindow.h"

#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <cstdlib>
#include <filesystem>
#include <framework/window/IWindowEvent.h>
#include <time.h>

namespace sky {

    namespace {
        void OnAppCommand(android_app *app, int32_t command)
        {
            if (app->userData != nullptr) {
                static_cast<AndroidPlatform *>(app->userData)->HandleCommand(command);
            }
        }
    } // namespace

    AndroidPlatform::~AndroidPlatform()
    {
        if (app != nullptr && app->userData == this) {
            app->onAppCmd = nullptr;
            app->userData = nullptr;
        }
    }

    void AndroidPlatform::HandleCommand(int32_t command)
    {
        switch (command) {
        case APP_CMD_INIT_WINDOW:
            SetMainWinHandle(app->window);
            if (mainWindow != nullptr && !IsLaunched()) {
                Launch();
            }
            break;
        case APP_CMD_TERM_WINDOW: SetMainWinHandle(nullptr); break;
        case APP_CMD_RESUME: lifecycle.Apply(AndroidLifecycleEvent::RESUME); break;
        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
            lifecycle.Apply(AndroidLifecycleEvent::PAUSE);
            if (nativeWindow != nullptr) {
                nativeWindow->CancelInput();
                nativeWindow->PollInput(app, false);
            }
            break;
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS: {
            const bool focus = command == APP_CMD_GAINED_FOCUS;
            lifecycle.Apply(focus ? AndroidLifecycleEvent::FOCUS_GAINED : AndroidLifecycleEvent::FOCUS_LOST);
            if (nativeWindow != nullptr) {
                if (!focus) {
                    nativeWindow->CancelInput();
                    nativeWindow->PollInput(app, false);
                }
                Event<IWindowEvent>::BroadCast(nativeWindow, &IWindowEvent::OnFocusChanged, focus);
            }
            break;
        }
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            if (nativeWindow != nullptr) {
                nativeWindow->UpdateMetrics(app->config);
            }
            break;
        case APP_CMD_DESTROY:
            lifecycle.Apply(AndroidLifecycleEvent::DESTROY);
            SetMainWinHandle(nullptr);
            if (nativeWindow != nullptr) {
                Event<IWindowEvent>::BroadCast(nativeWindow, &IWindowEvent::OnWindowClose, nativeWindow);
            }
            break;
        default: break;
        }
    }

    void AndroidPlatform::SetMainWinHandle(ANativeWindow *handle)
    {
        mainWindow = handle;
        lifecycle.Apply(handle != nullptr ? AndroidLifecycleEvent::SURFACE_AVAILABLE : AndroidLifecycleEvent::SURFACE_LOST);
        if (nativeWindow != nullptr) {
            nativeWindow->SetWinHandle(handle);
            if (handle == nullptr) {
                nativeWindow->PollInput(app, false);
            }
            nativeWindow->UpdateMetrics(app->config);
        }
    }

    void AndroidPlatform::SetNativeWindow(AndroidWindow *window)
    {
        nativeWindow = window;
    }

    NativeWindow *AndroidPlatform::GetMainWindow() const
    {
        return nativeWindow;
    }

    void AndroidPlatform::Launch()
    {
        if (launchCallback && !launched) {
            launched = true;
            launchCallback();
        }
    }

    bool AndroidPlatform::IsLaunched() const
    {
        return launched;
    }

    bool AndroidPlatform::IsFrameActive() const
    {
        return lifecycle.IsFrameActive();
    }

    uint64_t AndroidPlatform::GetFrameEpoch() const
    {
        return lifecycle.GetEpoch();
    }

    bool AndroidPlatform::Init(const PlatformInfo &desc)
    {
        app = static_cast<android_app *>(desc.application);
        if (app == nullptr || app->activity == nullptr) {
            app = nullptr;
            return false;
        }
        app->userData = this;
        app->onAppCmd = OnAppCommand;
        android_app_set_motion_event_filter(app, nullptr); // Include mouse/scroll, not just touch.
        GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_VSCROLL);
        GameActivity_setWindowFlags(app->activity, GAMEACTIVITY_FLAG_KEEP_SCREEN_ON | GAMEACTIVITY_FLAG_FULLSCREEN, 0);
        if (app->activityState == APP_CMD_RESUME) {
            lifecycle.Apply(AndroidLifecycleEvent::RESUME);
        }
        if (app->window != nullptr) {
            SetMainWinHandle(app->window);
        }
        perfManager = std::make_unique<AndroidPerfManager>();
        perfManager->Init();
        return true;
    }

    void AndroidPlatform::PollEvent(bool &exit)
    {
        if (app == nullptr || app->destroyRequested != 0 || lifecycle.IsDestroyed()) {
            exit = true;
            return;
        }
        if (mainWindow != nullptr && !launched) {
            Launch();
        }
        int timeout = IsFrameActive() ? 0 : -1;
        while (!exit) {
            int                  events = 0;
            android_poll_source *source = nullptr;
            const int            result = ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void **>(&source));
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested != 0 || lifecycle.IsDestroyed() || result == ALOOPER_POLL_ERROR) {
                exit = true;
                break;
            }
            timeout = 0; // Drain pending commands, then let the frame readiness gate run.
            if (result == ALOOPER_POLL_TIMEOUT || result == ALOOPER_POLL_WAKE) {
                break;
            }
        }
        if (nativeWindow != nullptr) {
            nativeWindow->PollInput(app, !exit && IsFrameActive() && lifecycle.HasFocus());
        } else if (auto *buffer = android_app_swap_input_buffers(app)) {
            android_app_clear_key_events(buffer);
            android_app_clear_motion_events(buffer);
        }
    }

    void AndroidPlatform::StartTextInput()
    {
        if (app != nullptr) {
            if (nativeWindow != nullptr) {
                nativeWindow->ResetTextInput();
            }
            const GameTextInputState empty = {"", 0, {0, 0}, {SPAN_UNDEFINED, SPAN_UNDEFINED}};
            GameActivity_setTextInputState(app->activity, &empty);
            GameActivity_showSoftInput(app->activity, GAMEACTIVITY_SHOW_SOFT_INPUT_IMPLICIT);
        }
    }

    void AndroidPlatform::StopTextInput()
    {
        if (app != nullptr) {
            if (nativeWindow != nullptr) {
                nativeWindow->EndTextInput();
            }
            GameActivity_hideSoftInput(app->activity, GAMEACTIVITY_HIDE_SOFT_INPUT_NOT_ALWAYS);
        }
    }

    uint64_t AndroidPlatform::GetPerformanceFrequency() const
    {
        return 1000000000ULL;
    }

    uint64_t AndroidPlatform::GetPerformanceCounter() const
    {
        timespec time = {};
        if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) {
            return 0;
        }
        return static_cast<uint64_t>(time.tv_sec) * 1000000000ULL + static_cast<uint64_t>(time.tv_nsec);
    }

    std::string AndroidPlatform::GetInternalPath() const
    {
        return app != nullptr && app->activity->internalDataPath != nullptr ? app->activity->internalDataPath : "";
    }

    std::string AndroidPlatform::GetBundlePath() const
    {
        // APK assets have no native directory; callers must use GetBundleFileSystem().
        return {};
    }

    std::string AndroidPlatform::GetUserConfigPath() const
    {
        const std::string internal = GetInternalPath();
        if (internal.empty()) {
            return {};
        }
        const auto      directory = std::filesystem::path(internal) / "config";
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        return error ? std::string{} : directory.string() + "/";
    }

    std::string AndroidPlatform::GetEnvVariable(const std::string &name) const
    {
        const char *value = std::getenv(name.c_str());
        return value != nullptr ? value : "";
    }

    PlatformType AndroidPlatform::GetType() const
    {
        return PlatformType::Android;
    }

    void *AndroidPlatform::GetMainWinHandle() const
    {
        return mainWindow;
    }

    void *AndroidPlatform::GetNativeApp() const
    {
        return app;
    }

    AdaptivePerfManager *AndroidPlatform::GetPerformanceManager() const
    {
        return perfManager.get();
    }

    FileSystemPtr AndroidPlatform::GetBundleFileSystem()
    {
        if (!assetFs) {
            assetFs = new AndroidBundleFileSystem("");
        }
        return assetFs;
    }

    bool Platform::Init(const PlatformInfo &info)
    {
        platform = std::make_unique<AndroidPlatform>();
        return platform->Init(info);
    }

} // namespace sky
