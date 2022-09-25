//
// Created by Zach Lee on 2021/11/9.
//

#include "Command.h"
#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>

int main(int argc, char **argv)
{
    sky::CommandInfo cmdInfo = {};
    sky::ProcessCommand(argc, argv, cmdInfo);

    sky::PlatformBase* platform = sky::PlatformBase::GetPlatform();
    if (!platform->Init()) {
        return 1;
    }

    sky::StartInfo start = {};
    start.appName        = "Win32Launcher";
    start.modules.swap(cmdInfo.modules);

    sky::GameApplication app;
    if (app.Init(start)) {
        app.Mainloop();
    }

    app.Shutdown();

    platform->Shutdown();

    return 0;
}