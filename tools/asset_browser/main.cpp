#include <iostream>
#include <cxxopts.hpp>

#include <framework/application/Application.h>
#include <framework/application/ToolApplicationBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>
using namespace sky;

int main(int argc, char *argv[])
{
    cxxopts::Options options("AssetBrowser", "SkyEngine AssetBrowser");
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

    auto* am = AssetManager::Get();
    std::vector<Uuid> importFileList;
    if (result.count("list") != 0u) {
        const auto &idMap = am->GetIDMap();
        printf("Asset List: (item %d)\n", static_cast<uint32_t>(idMap.size()));
        for (const auto &[id, info] : idMap) {
            printf("UUID: %s\t hash: %u,\t path: %s,\n", id.ToString().c_str(), info.hash, info.loadPath.c_str());
        }
        goto EXIT;
    }

    if (result.count("imports") != 0U) {
        auto file = result["imports"].as<std::string>();
        std::ifstream f(projectPath + "/" + file);

        std::string line;
        while (std::getline(f, line)) {
            am->ImportAndBuildAsset(line);
        }
    }

    if (result.count("import") != 0U) {
        auto list = result["import"].as<std::vector<std::string>>();
        for (const auto &path : list) {
            am->ImportAndBuildAsset(path);
        }
        goto EXIT;
    }

EXIT:
    app.Shutdown();
    return 0;
}