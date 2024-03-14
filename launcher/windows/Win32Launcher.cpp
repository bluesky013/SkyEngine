//
// Created by Zach Lee on 2021/11/9.
//

#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>
#include <framework/application/XRApplication.h>

#include <cxxopts.hpp>
#include <filesystem>

using namespace sky;

int main(int argc, char **argv)
{
    // platform
    sky::Platform* platform = sky::Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    cxxopts::Options options("SkyEngine Launcher", "SkyEngine Launcher");
    options.allow_unrecognised_options();
    options.add_options()("a, app", "app mode", cxxopts::value<std::string>());
    auto result = options.parse(argc, argv);
    bool isXRMode = (result.count("app") != 0u) && result["app"].as<std::string>() == "xr";
    if (isXRMode) {
        sky::XRApplication app;
        if (app.Init(argc, argv)) {
            app.Mainloop();
        }
        app.Shutdown();
    } else {
        sky::GameApplication app;
        if (app.Init(argc, argv)) {
            app.Mainloop();
        }
        app.Shutdown();
    }


    return 0;
}
