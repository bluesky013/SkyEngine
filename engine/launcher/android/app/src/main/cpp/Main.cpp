//
// Android launcher entry: bridges GameActivity (via android_native_app_glue)
// into the engine Application. Rendering is provided by the aurora runtime
// module loaded by GameApplication (no legacy render dependency).
//

#include <framework/application/GameApplication.h>
#include <framework/platform/PlatformBase.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>

// Glue from GameActivity to android_main(): the .c is compiled into this TU.
extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

void android_main(struct android_app *app)
{
    sky::Platform *platform = sky::Platform::Get();
    platform->Init({app});

    sky::GameApplication application;
    bool                started = false;

    platform->setLaunchCallback([&application, &started]() {
        application.Init(0, nullptr);
        started = true;
    });

    do {
        int                         events = 0;
        struct android_poll_source *source = nullptr;
        int                         result = 0;
        do {
            result = ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void **>(&source));
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
