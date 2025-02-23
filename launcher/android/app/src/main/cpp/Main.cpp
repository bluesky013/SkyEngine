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
    sky::Platform* platform = sky::Platform::Get();
    platform->Init({app});

    sky::GameApplication application;
    bool started = false;
    platform->setLaunchCallback([&application, &started, platform]() {
        std::vector<char *> args;

        application.Init(static_cast<int>(args.size()), args.data());
        started = true;
    });

//    auto *perfManager = platform->GetPerformanceManager();
//    auto *iThermal = perfManager->GetIThermal();
//    if (iThermal != nullptr) {
//        iThermal->RegisterStatusChangeCallback("Key", [](sky::ThermalStatus status) {
//
//        });
//    }

    do {
        int events;
        struct android_poll_source *source;

        int result;
        do {
            result = ALooper_pollOnce(0, nullptr, &events, (void **) &source);

            if (source != nullptr) {
                source->process(app, source);
            }
        } while (result == ALOOPER_POLL_CALLBACK);

        if (started) {
            application.Loop();
        }
    } while (app->destroyRequested == 0);

    platform->Shutdown();
}