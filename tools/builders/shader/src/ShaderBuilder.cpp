//
// Created by Zach Lee on 2022/1/31.
//

#include <shader/ShaderBuilder.h>
#include <framework/asset/AssetManager.h>
#include <ProjectRoot.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <core/file/FileUtil.h>

using namespace rapidjson;

static const char* TAG = "ShaderLoader";

namespace sky {

    static const char* PRG_EXT = ".prog";
    static const char* VS_EXT = ".vert";
    static const char* FS_EXT = ".frag";

    static bool ParseShader(const std::string& tag, Document& document, ShaderSourceData& data)
    {
        if(!document.HasMember(tag.data())) {
            return false;
        }
        ShaderData shader;

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
        ReadBin(std::string(path) + ".spv", shader.data);

        if (val.HasMember("entry")) {
            shader.entry = val["entry"].GetString();
        }

        data.shaders.emplace_back(shader);
        return true;
    }

    ShaderBuilder::ShaderBuilder()
    {
    }

    ShaderBuilder::~ShaderBuilder()
    {
    }

    bool ShaderBuilder::Build(const BuildRequest& request)
    {
        std::string path;
        if (!ConstructFullPath(request.srcFolder, request.srcFile, path)) {
            return false;
        }

        std::string data;
        if (!ReadString(path, data)) {
            return false;
        }

        Document document;
        document.Parse(data.data());

        if (document.HasParseError()) {
            LOG_E(TAG, "parse json failed, %u", document.GetParseError());
            return false;
        }

        ShaderSourceData sourceData;
        ParseShader("vert", document, sourceData);
        ParseShader("frag", document, sourceData);
        return true;
    }

    bool ShaderBuilder::Support(const std::string& ext) const
    {
        return ext == PRG_EXT;
    }
}