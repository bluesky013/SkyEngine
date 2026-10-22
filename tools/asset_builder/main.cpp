#include <iostream>
#include <fstream>
#include <cxxopts.hpp>

#include <framework/application/Application.h>
#include <framework/application/ToolApplicationBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/platform/PlatformBase.h>
using namespace sky;

int main(int argc, char *argv[])
{
    cxxopts::Options options("AssetBuilder", "SkyEngine AssetBuilder");
    options.add_options()("e,engine", "Engine Directory", cxxopts::value<std::string>())
        ("p,project", "Project Directory", cxxopts::value<std::string>())
        ("i,import", "Import Source Asset", cxxopts::value<std::vector<std::string>>())
        ("f,imports", "Import Source Asset by list", cxxopts::value<std::string>())
        ("l,list", "Project Asset List")
        ("h,help", "Print usage");
    options.allow_unrecognised_options();

    auto result = options.parse(argc, argv);
    if (result.count("help") != 0u) {
        printf("%s\n", options.help().c_str());
        exit(0);
    }

    if (result.count("project") == 0u) {
        printf("project path not specified\n");
        return -1;
    }

    // platform
    sky::Platform* platform = sky::Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    sky::ToolApplicationBase app;
    if (!app.Init(argc, argv)) {
        return -1;
    }

    std::string projectPath = result["project"].as<std::string>();

    if (result.count("list") != 0u) {
        auto sources = AssetDataBase::Get()->Gather("");
        printf("Asset List: (item %u)\n", static_cast<uint32_t>(sources.size()));
        for (const auto &src : sources) {
            printf("UUID: %s\t path: %s\n", src->uuid.ToString().c_str(), src->path.path.GetStr().c_str());
        }
        return 0;
    }

    std::vector<std::string> importList;
    if (result.count("imports") != 0U) {
        auto file = result["imports"].as<std::string>();
        std::ifstream f(projectPath + "/" + file);
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty()) {
                importList.emplace_back(line);
            }
        }
    }
    if (result.count("import") != 0U) {
        auto list = result["import"].as<std::vector<std::string>>();
        importList.insert(importList.end(), list.begin(), list.end());
    }

    for (const auto &path : importList) {
        FilePath srcPath(path);
        if (!path.empty() && path.front() != '/') {
            srcPath = FilePath(projectPath) / srcPath;
        }
        AssetImportRequest request = {};
        request.filePath = srcPath;
        request.savePath = srcPath;
        AssetBuilderManager::Get()->ImportAsset(request);
        printf("Import queued: %s\n", srcPath.GetStr().c_str());
    }
    AssetExecutor::Get()->WaitForAll();

    // 落盘资产注册表，游戏运行时（AssetDataBase::Load）依赖它
    AssetDataBase::Get()->Save();

    return 0;
}
