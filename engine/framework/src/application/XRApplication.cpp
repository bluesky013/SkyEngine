//
// Created by blues on 2024/3/13.
//

#include <framework/application/XRApplication.h>
#include <core/cmdline/CmdParser.h>

#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <filesystem>

#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "Application";
static const char *CONFIG_PATH = "/config/modules_game.json";

namespace sky {

    bool XRApplication::Init(int argc, char **argv)
    {
        if (!Application::Init(argc, argv)) {
            return false;
        }

        return true;
    }

    bool XRApplication::LoadConfigs()
    {
        return true;
    }


} // namespace sky