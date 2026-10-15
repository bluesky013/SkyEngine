//
// Created by Zach Lee on 2021/11/9.
//

#include <core/cmdline/CmdParser.h>
#include <framework/application/GameApplication.h>
#include <framework/platform/PlatformBase.h>

#include <cstdint>

using namespace sky;

int main(int argc, char **argv)
{
    CmdOptions options("SkyEngine Launcher", "SkyEngine Launcher");
    options.allow_unrecognised_options();
    options.add_options()("frames", "Run a bounded number of frames", CmdValue<uint32_t>());
    const auto     arguments = options.parse(argc, argv);
    const uint32_t frames    = arguments.count("frames") != 0 ? arguments["frames"].as<uint32_t>() : 0;

    sky::Platform *platform = sky::Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    sky::GameApplication app;
    if (!app.Init(argc, argv)) {
        return 1;
    }
    if (frames == 0) {
        app.Mainloop();
    } else {
        for (uint32_t frame = 0; frame < frames && !app.IsExit(); ++frame) {
            app.Loop();
        }
    }
    return 0;
}
