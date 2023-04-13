//
// Created by bluesky on 2022/9/25.
//

#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>
#include <android/window.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

// Glue from GameActivity to android_main()
// Passing GameActivity event from main thread to app native thread.
extern  "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

void android_main(struct android_app *app) {
    sky::PlatformBase* platform = sky::PlatformBase::GetPlatform();
    if (!platform->Init({app})) {
        return;
    }

    sky::StartInfo start = {};
    start.appName        = "AndroidLauncher";
    start.modules.emplace_back("RHISample");

    auto path = app->activity->internalDataPath;

    sky::GameApplication application;
    application.Init(start);

    do {
        int events;
        struct android_poll_source *source;

        while ((ALooper_pollAll(0, nullptr, &events, (void **) &source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
        }

        application.Loop();
    } while (app->destroyRequested == 0);

    application.Shutdown();
    platform->Shutdown();
}