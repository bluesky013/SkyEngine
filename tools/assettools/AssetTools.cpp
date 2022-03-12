//
// Created by Zach Lee on 2021/12/6.
//

#include <iostream>
#include <ProjectRoot.h>

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <core/file/FileUtil.h>
#include <core/file/FileIO.h>

#include <framework/Application.h>

using namespace sky;
using namespace rapidjson;

static const char* ASSET_CONFIG= "asset.json";

int main()
{
    std::string path;
    if (!ConstructFullPath(PROJECT_ROOT, ASSET_CONFIG, path)) {
        return false;
    }

    std::string data;
    if (!ReadString(path, data)) {
        return false;
    }

    Document document;
    document.Parse(data.data());

    std::vector<std::string> folders;

    sky::StartInfo start = {};
    start.appName = "AssetTool";
    start.modules.emplace_back("AssetToolModule");

    auto parseStringArray = [&document](const char* str, std::vector<std::string>& out) {
        if (document.HasMember(str)) {
            auto& values = document[str];
            if (values.IsArray()) {
                auto array = values.GetArray();
                for (auto& value : array) {
                    out.emplace_back(value.GetString());
                }
            }
        }
    };
    parseStringArray("folders", folders);
    parseStringArray("builders", start.modules);

    sky::Application app;
    if (app.Init(start)) {
        app.Mainloop();
    }

    app.Shutdown();

    return 0;
}