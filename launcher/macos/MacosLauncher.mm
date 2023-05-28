//
// Created by Zach Lee on 2021/11/28.
//

#include "Command.h"
#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>

int main(int argc, char **argv)
{
    sky::CommandInfo cmdInfo = {};
    sky::ProcessCommand(argc, argv, cmdInfo);

    sky::Platform *platform = sky::Platform::Get();
    if (!platform->Init({})) {
        return 1;
    }

    sky::StartInfo start = {};
    start.appName        = "MacosLauncher";
    start.modules.swap(cmdInfo.modules);

    sky::GameApplication app;
    for (auto &[key, value] : cmdInfo.values) {
        start.setting.SetValue(key, value);
    }

    if (app.Init(start)) {
        app.Mainloop();
    }

    app.Shutdown();

    platform->Shutdown();

    return 0;
}
