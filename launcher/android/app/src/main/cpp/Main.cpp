//
// Created by bluesky on 2022/9/25.
//

#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>
#include <android/window.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
extern "C" {
void android_main(struct android_app* app);
}

void android_main(struct android_app *app) {
    sky::PlatformBase* platform = sky::PlatformBase::GetPlatform();
    if (!platform->Init({app})) {
        return;
    }

//    sky::StartInfo start = {};
//    start.appName        = "AndroidLauncher";
//
//    sky::GameApplication game;
//    if (game.Init(start)) {
//        game.Mainloop();
//    }
//
//    game.Shutdown();

//    while (true) {
//        int events;
//        struct android_poll_source *source;
//
//        while ((ALooper_pollAll(0, nullptr, &events, (void **) &source)) >= 0) {
//            if (source != nullptr) {
//                source->process(app, source);
//            }
//
//            if (app->destroyRequested != 0) {
//                return;
//            }
//        }
//    }

    platform->Shutdown();
}