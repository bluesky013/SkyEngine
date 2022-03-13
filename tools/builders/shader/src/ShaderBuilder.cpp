//
// Created by Zach Lee on 2022/1/31.
//

#include <shader/ShaderBuilder.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <core/file/FileUtil.h>
#include <shaderc/shaderc.hpp>
#include <vector>
#include <filesystem>

using namespace rapidjson;

static const char* TAG = "ShaderLoader";

namespace sky {

    static const char* PRG_EXT = ".prog";
    static const char* VS_EXT = ".vert";
    static const char* FS_EXT = ".frag";

    void CompileShader(const std::string& name, shaderc_shader_kind kind, const std::string& data,
        std::vector<uint32_t>& out)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(data, kind, name.c_str(), options);
        out.assign(result.begin(), result.end());
    }

    static bool ParseShader(std::filesystem::path path, const std::string& tag, Document& document,
        ShaderSourceData& data)
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

        path.append(val["path"].GetString());
        std::string source;
        if (!ReadString(path.string(), source)) {
            return false;
        }


//        ReadBin(std::string(path) + ".spv", shader.data);
//
//        if (val.HasMember("entry")) {
//            shader.entry = val["entry"].GetString();
//        }
//
//        data.shaders.emplace_back(shader);
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
        std::string data;
        if (!ReadString(request.source.string(), data)) {
            return false;
        }

        Document document;
        document.Parse(data.data());

        if (document.HasParseError()) {
            LOG_E(TAG, "parse json failed, %u", document.GetParseError());
            return false;
        }
        auto parentPath = request.source.parent_path();

        ShaderSourceData sourceData;
        ParseShader(parentPath, "vert", document, sourceData);
        ParseShader(parentPath, "frag", document, sourceData);
        return true;
    }

    bool ShaderBuilder::Support(const std::string& ext) const
    {
        return ext == PRG_EXT;
    }
}