//
// Created by Zach Lee on 2022/1/31.
//

#include <shader/ShaderLoader.h>
#include <engine/BasicSerialization.h>
#include <framework/asset/AssetManager.h>
#include <ProjectRoot.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <core/logger/Logger.h>
#include <core/file/FileIO.h>

using namespace rapidjson;

static const char* TAG = "ShaderLoader";

namespace sky {

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

    ShaderLoader::ShaderLoader()
    {
        AssetManager::Get()->RegisterHandler<ShaderAsset>();
    }

    ShaderLoader::~ShaderLoader()
    {
        AssetManager::Get()->UnRegisterHandler<ShaderAsset>();
    }

    bool ShaderLoader::Load(const std::string &path)
    {
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

        asset = AssetManager::Get()->FindOrCreate<ShaderAsset>(Uuid::Create());

        ShaderSourceData& sourceData = asset->sourceData;
        ParseShader("vert", document, sourceData);
        ParseShader("frag", document, sourceData);
        return true;
    }

    void ShaderLoader::Save(const std::string& path)
    {
        AssetManager::Get()->SaveAsset(path, asset, ShaderAsset::TYPE);
    }
}