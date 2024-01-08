//
// Created by Zach Lee on 2021/11/9.
//

#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>

#include <filesystem>

using namespace sky;

int main(int argc, char **argv)
{
    sky::GameApplication app;
    if (app.Init(argc, argv)) {
        app.Mainloop();
    }

    app.Shutdown();
    return 0;
}
