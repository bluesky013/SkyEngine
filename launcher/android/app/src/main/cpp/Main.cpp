//
// Created by bluesky on 2022/9/25.
//

#include <android/window.h>
#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>
#include <game-activity/native_app_glue/android_native_app_glue.h>
extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>

void android_main(struct android_app* app);
}


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

void android_main(struct android_app *app) {
    app->onAppCmd = HandleCommand;
    auto *activity = app->activity;

    GameActivity_setWindowFlags(
            activity,
            AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_TURN_SCREEN_ON |
            AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_SHOW_WHEN_LOCKED,
            0);

    while (true) {
        int events;
        struct android_poll_source *source;

        while ((ALooper_pollAll(0, nullptr, &events, (void **) &source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }

            if (app->destroyRequested != 0) {
                return;
            }
        }
    }
}