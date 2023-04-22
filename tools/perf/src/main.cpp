//
// Created by Zach Lee on 2023/4/21.
//

#include <stdio.h>

#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>
#include <perf/ADB.h>
#include <perf/Module.h>

using namespace sky;

int main(int argc, char **argv)
{
    Platform::Get()->Init({});

    StartInfo start = {};
    start.appName        = "PerfCat";

    GameApplication app;
    app.RegisterModule(std::make_unique<perf::Module>());

    if (app.Init(start)) {
        app.Mainloop();
    }

    app.Shutdown();
    Platform::Get()->Shutdown();
    return 0;
}
