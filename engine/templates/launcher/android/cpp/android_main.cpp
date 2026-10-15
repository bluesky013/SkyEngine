//
// Android launcher entry: bridges GameActivity (via android_native_app_glue)
// into the engine Application. Rendering is provided by the aurora runtime
// module loaded by GameApplication (no legacy render dependency).
//

#include <framework/application/GameApplication.h>
#include <framework/platform/PlatformBase.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>

void android_main(struct android_app *app)
{
    sky::Platform *platform = sky::Platform::Get();
    if (!platform->Init({app})) {
        if (app != nullptr && app->activity != nullptr) {
            GameActivity_finish(app->activity);
        }
        return;
    }

    // Destroy the application (modules/windows) before its platform callbacks.
    sky::GameApplication application;
    bool                 started = false;
    bool                 failed  = false;

    platform->setLaunchCallback([&application, &started, &failed, app]() {
        started = application.Init(0, nullptr);
        failed  = !started;
        if (failed) {
            GameActivity_finish(app->activity);
        }
    });

    bool quit = false;
    while (!quit && app->destroyRequested == 0 && !failed) {
        if (started) {
            application.Loop();
            quit = application.IsExit();
        } else {
            platform->PoolEvent(quit);
        }
    }
    if (app->destroyRequested == 0 && !failed) {
        GameActivity_finish(app->activity);
    }
}
