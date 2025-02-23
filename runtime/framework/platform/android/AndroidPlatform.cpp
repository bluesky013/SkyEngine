//
// Created by Zach Lee on 2022/9/26.
//

#include "AndroidPlatform.h"
#include <android/window.h>
#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

static const char* TAG = "AndroidPlatform";

namespace sky {

    void HandleCommand(struct android_app* app, int32_t cmd)
    {
        AndroidPlatform *platform = static_cast<AndroidPlatform*>(app->userData);

        switch (cmd) {
            case APP_CMD_SAVE_STATE:
                break;
            case APP_CMD_INIT_WINDOW:
                if (!platform->IsLaunched()) {
                    platform->SetMainWinHandle(app->window);
                    platform->Launch();
                }

                break;
//        case APP_CMD_TERM_WINDOW:
//            break;
//        case APP_CMD_GAINED_FOCUS:
//            break;
//        case APP_CMD_LOST_FOCUS:
//            break;
//        case APP_CMD_PAUSE:
//            break;
//        case APP_CMD_RESUME:
//            break;
//        case APP_CMD_STOP:
//            break;
//        case APP_CMD_START:
//            break;
//        case APP_CMD_WINDOW_RESIZED:
//        case APP_CMD_CONFIG_CHANGED:
//            break;
//        case APP_CMD_LOW_MEMORY:
//            break;
            default:
                break;
        }
    }

    void AndroidPlatform::SetMainWinHandle(ANativeWindow *handle)
    {
        mainWindow = handle;
    }

    void AndroidPlatform::Launch()
    {
        if (launchCallback) {
            launchCallback();
        }
        launched = true;
    }

    bool AndroidPlatform::IsLaunched() const
    {
        return launched;
    }

    bool AndroidPlatform::Init(const PlatformInfo& desc)
    {
        app = reinterpret_cast<android_app*>(desc.application);
        app->userData = this;
        app->onAppCmd = HandleCommand;
        auto *activity = app->activity;

        GameActivity_setWindowFlags(
                activity,
                AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_TURN_SCREEN_ON |
                AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_SHOW_WHEN_LOCKED,
                0);

        perfManager = std::make_unique<AndroidPerfManager>();
        perfManager->Init();
        return true;
    }

    uint64_t AndroidPlatform::GetPerformanceFrequency() const
    {
        return 0;
    }

    uint64_t AndroidPlatform::GetPerformanceCounter() const
    {
        return 0;
    }

    std::string AndroidPlatform::GetInternalPath() const
    {
        return app->activity->internalDataPath;
    }

    std::string AndroidPlatform::GetBundlePath() const
    {
        return "";
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
            assetFs = new AndroidBundleFileSystem();
        }
        return assetFs;
    }

    bool Platform::Init(const PlatformInfo& info)
    {
        platform = std::make_unique<AndroidPlatform>();
        return platform->Init(info);
    }

}
