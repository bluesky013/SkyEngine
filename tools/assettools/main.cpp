//
// Created by Zach Lee on 2022/8/12.
//

#include <iostream>
#include <filesystem>
#include <cxxopts.hpp>
#include <framework/platform/PlatformBase.h>
#include <framework/application/Application.h>
#include <framework/asset/AssetManager.h>
#include <framework/database/DBManager.h>
#include <framework/serialization/CoreReflection.h>
using namespace sky;

int main(int argc, char *argv[])
{
    cxxopts::Options options("AssetBuilder", "SkyEngine AssetProcessor");
    options.add_options()
        ("e,engine", "Engine Directory", cxxopts::value<std::string>())
        ("p,project", "Project Directory", cxxopts::value<std::string>())
        ("o,file", "Single Asset File", cxxopts::value<std::string>())
        ("h,help", "Print usage");

    auto result = options.parse(argc, argv);
    if (result.count("help") != 0u) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    Platform* platform = Platform::Get();
    if (!platform->Init({})) {
        return 1;
    }
    StartInfo start = {};
    start.appName        = "AssetTool";
    start.modules.emplace_back("RenderBuilder");

    Application app;
    app.Init(start);

    std::string projectPath;
    std::string enginePath;
    std::string filePath;
    if (result.count("project") != 0u) {
        projectPath = result["project"].as<std::string>();
    }

    if (result.count("engine") != 0u) {
        enginePath = result["engine"].as<std::string>() + "/assets";
    }

    if (result.count("file") != 0u) {
        filePath = result["file"].as<std::string>();
    }

    std::cout << "Engine Path: " << enginePath << std::endl;
    std::cout << "Project Path: " << projectPath << std::endl;

    auto *am = AssetManager::Get();
    am->RegisterSearchPath(projectPath + "/assets");
    am->RegisterSearchPath(enginePath);
    am->Reset(projectPath + "/assets.db");
    Interface<ISystemNotify>::Get()->GetApi()->GetSettings().SetValue("PROJECT_PATH", projectPath);

    am->ImportSource(filePath, {});

//    std::vector<std::filesystem::path> pathList;
//    pathList.emplace_back(projectPath);
//    pathList.emplace_back(enginePath);
//
//    for (auto& path : pathList) {
//        for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
//            auto file = std::filesystem::absolute(entry.path());
//            if (is_directory(file)) {
//                continue;
//            }
//            builderManager.Build(projectPath, file.string());
//        }
//    }

    app.Shutdown();
    platform->Shutdown();
    return 0;
}
