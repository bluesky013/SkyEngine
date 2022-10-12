//
// Created by Zach Lee on 2022/9/26.
//

#include "AndroidPlatform.h"

#include <android/window.h>
#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>
extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

static const char* TAG = "AndroidPlatform";

namespace sky {

    void HandleCommand(struct android_app* app, int32_t cmd)
    {
        switch (cmd) {
            case APP_CMD_SAVE_STATE:
                break;
            case APP_CMD_INIT_WINDOW:
                if (app->window != nullptr) {
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

    PlatformBase *PlatformBase::GetPlatform()
    {
        static AndroidPlatform platform;
        return &platform;
    }

    bool AndroidPlatform::Init(const Descriptor& desc)
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
        return true;
    }

    void AndroidPlatform::Shutdown()
    {

    }

    uint64_t AndroidPlatform::GetPerformanceFrequency() const
    {
        return 0;
    }

    uint64_t AndroidPlatform::GetPerformanceCounter() const
    {
        return 0;
    }

}
