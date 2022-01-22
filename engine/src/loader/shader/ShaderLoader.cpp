//
// Created by Zach Lee on 2022/1/16.
//

#include <engine/loader/shader/ShaderLoader.h>
#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <ProjectRoot.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <filesystem>
using namespace rapidjson;

static const char* TAG = "ShaderLoader";

namespace sky {

    static bool ParseShader(std::filesystem::path parentPath, const std::string& tag, Document& document, ShaderAsset::SourceData& data)
    {
        if(!document.HasMember(tag.data())) {
            return false;
        }
        ShaderAsset::ShaderData shader;

        auto& val = document[tag.data()];
        if (tag == "vert") {
            shader.stage = VK_SHADER_STAGE_VERTEX_BIT;
        } else if (tag == "frag") {
            shader.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        } else if (tag == "comp") {
            shader.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        if (!val.HasMember("path")) {
            return false;
        }
        auto path = val["path"].GetString();
        parentPath.append(std::string(path) + ".spv");
        ReadBin(parentPath.string(), shader.data);

        if (val.HasMember("entry")) {
            shader.entry = val["entry"].GetString();
        }

        data.shaders.emplace_back(shader);
        return true;
    }

    void ShaderLoader::Load(const std::string &path, ShaderAsset::SourceData& sourceData)
    {
        std::string data;
        if (!ReadString(path, data)) {
            return;
        }

        Document document;
        document.Parse(data.data());

        if (document.HasParseError()) {
            LOG_E(TAG, "parse json failed, %u", document.GetParseError());
            return;
        }
        std::filesystem::path absolutePath(path);
        auto rootDir = absolutePath.parent_path();
        ParseShader(rootDir, "vert", document, sourceData);
        ParseShader(rootDir, "frag", document, sourceData);
    }

}