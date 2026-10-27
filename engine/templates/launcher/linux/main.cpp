//
// Project-owned Linux launcher entry.
//

#include <framework/platform/PlatformBase.h>
#include <framework/application/GameApplication.h>
#include <framework/application/XRApplication.h>

#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>

#ifdef SKY_EDITOR_HOST
#include "EditorApplication.h"
#endif

using namespace sky;

int main(int argc, char **argv)
{
    sky::Platform *platform = sky::Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    CmdOptions options("SkyEngine Launcher", "SkyEngine Launcher");
    options.allow_unrecognised_options();
    options.add_options()("a, app", "app mode", CmdValue<std::string>());
    auto result = options.parse(argc, argv);
    const std::string appMode = result.count("app") != 0u ? result["app"].as<std::string>() : std::string();
    const bool isXRMode = appMode == "xr";
    const bool isEditorMode = appMode == "editor";

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
            LOG_E("Launcher", "editor mode unavailable: build with SKY_BUILD_SANDBOX=ON");
        }
        sky::GameApplication app;
        if (app.Init(argc, argv)) {
            app.Mainloop();
        }
    }

    return 0;
}
