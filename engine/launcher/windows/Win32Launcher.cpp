//
// Created by Zach Lee on 2021/11/9.
//

#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>
#include <framework/application/XRApplication.h>

#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <filesystem>

#ifdef SKY_EDITOR_HOST
#include "EditorApplication.h"
#endif

using namespace sky;

int main(int argc, char **argv)
{
    // platform
    sky::Platform* platform = sky::Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    CmdOptions options("SkyEngine Launcher", "SkyEngine Launcher");
    options.allow_unrecognised_options();
    options.add_options()("a, app", "app mode", CmdValue<std::string>());
    auto result = options.parse(argc, argv);
    const std::string appMode = result.count("app") != 0u ? result["app"].as<std::string>() : std::string();
    bool isXRMode = appMode == "xr";
    bool isEditorMode = appMode == "editor";

    if (isXRMode) {
        sky::XRApplication app;
        if (app.Init(argc, argv)) {
            app.Mainloop();
        }
    }
#ifdef SKY_EDITOR_HOST
    else if (isEditorMode) {
        sky::editor::sandbox::EditorApplication app;
        if (app.Init(argc, argv)) {
            app.Mainloop();
        }
    }
#endif
    else {
        if (isEditorMode) {
            // Editor mode requested but the sandbox editor was not built in.
            LOG_E("Launcher", "editor mode unavailable: build with SKY_BUILD_SANDBOX=ON");
        }
        sky::GameApplication app;
        if (app.Init(argc, argv)) {
            app.Mainloop();
        }
    }

    return 0;
}
