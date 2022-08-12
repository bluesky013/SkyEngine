//
// Created by Zach Lee on 2022/8/12.
//

#include <iostream>
#include <filesystem>
#include <cxxopts.hpp>
#include <builders/BuilderManager.h>
#include <builders/model/ModelBuilder.h>

int main(int argc, char *argv[])
{
    cxxopts::Options options("AssetBuilder", "SkyEngine AssetProcessor");
    options.add_options()
        ("e,engine", "Engine Directory", cxxopts::value<std::string>())
        ("p,project", "Project Directory", cxxopts::value<std::string>())
        ("h,help", "Print usage");

    auto result = options.parse(argc, argv);
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    std::string projectPath = "";
    std::string enginePath = "";
    if (result.count("project")) {
        projectPath = result["project"].as<std::string>();
    }

    if (result.count("engine")) {
        enginePath = result["engine"].as<std::string>();
    }
    std::cout << "Engine Path: " << enginePath << std::endl;
    std::cout << "Project Path: " << projectPath << std::endl;

    sky::BuilderManager builderManager;
    builderManager.RegisterBuilder(new sky::ModelBuilder());

    std::filesystem::path path = projectPath;
    path.append("Assets");
    for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        auto file = std::filesystem::absolute(entry.path());
        if (is_directory(file)) {
            continue;
        }
        builderManager.Build(file.string());
    }

    return 0;
}