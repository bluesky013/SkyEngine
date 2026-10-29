//
// Android launcher entry: bridges GameActivity into the engine Application.
//

#include "AndroidLauncherLoop.h"
#include <android/looper.h>
#include <core/logger/Logger.h>
#include <framework/application/GameApplication.h>
#include <framework/platform/PlatformBase.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace {
    class AndroidApplicationHost {
    public:
        AndroidApplicationHost(android_app &app, sky::Platform *platform, sky::GameApplication *application)
            : app(app), platform(platform), application(application)
        {
        }
        bool DestroyRequested() const
        {
            return app.destroyRequested != 0;
        }
        void Launch()
        {
            started = application->Init(0, nullptr);
            failed  = !started;
        }
        bool RunApplicationStep()
        {
            if (!platform || !application || failed)
                return false;
            if (started) {
                application->Loop();
                return !application->IsExit();
            }
            bool exit = false;
            platform->PoolEvent(exit);
            return !exit && !failed;
        }
        void RequestFinish()
        {
            GameActivity_finish(app.activity);
        }
        void ShutdownApplication()
        {
            if (platform)
                platform->setLaunchCallback(sky::LaunchCallback{});
            if (application)
                application->Shutdown();
        }
        bool PollLifecycle()
        {
            if (platform) {
                bool exit = false;
                platform->PoolEvent(exit);
                return !exit || DestroyRequested();
            }
            int                  events = 0;
            android_poll_source *source = nullptr;
            const int            result = ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void **>(&source));
            if (source)
                source->process(&app, source);
            return result != ALOOPER_POLL_ERROR;
        }

    private:
        android_app          &app;
        sky::Platform        *platform;
        sky::GameApplication *application;
        bool                  started = false;
        bool                  failed  = false;
    };
} // namespace

void android_main(android_app *app)
{
    if (!app || !app->activity) {
        LOG_E("AndroidLauncher", "native entry requires a valid GameActivity");
        return;
    }
    auto *platform = sky::Platform::Get();
    if (!platform->Init({app})) {
        AndroidApplicationHost host(*app, nullptr, nullptr);
        if (!sky::detail::RunAndroidLauncherLoop(host))
            LOG_E("AndroidLauncher", "lifecycle polling failed before DESTROY after platform initialization failure");
        platform->Shutdown();
        return;
    }

    // The application/window and platform callbacks survive the finish handshake.
    sky::GameApplication   application;
    AndroidApplicationHost host(*app, platform, &application);
    platform->setLaunchCallback([&host] { host.Launch(); });
    if (!sky::detail::RunAndroidLauncherLoop(host))
        LOG_E("AndroidLauncher", "lifecycle polling failed before DESTROY");
}
