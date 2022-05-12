//
// Created by Zach Lee on 2022/1/31.
//

#include <shader/ShaderBuilder.h>
#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <core/file/FileUtil.h>
#include <filesystem>

using namespace rapidjson;

static const char* TAG = "ShaderLoader";

namespace sky {

    static const char* PRG_EXT = ".prog";
    static const char* VS_EXT = ".vert";
    static const char* FS_EXT = ".frag";

    class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
    public:
        shaderc_include_result* GetInclude(const char* requested_source,
            shaderc_include_type type,
            const char* requesting_source,
            size_t include_depth) override
        {
            return new shaderc_include_result();
        }

        void ReleaseInclude(shaderc_include_result* data)
        {
            delete data;
        }

        virtual ~ShaderIncluder() = default;
    };

//    void ShaderBuilder::CompileShader(const std::string& name, shaderc_shader_kind kind, const std::string& data,
//        ShaderData& shaderOut, const shaderc::CompileOptions& options)
//    {
//        shaderc::SpvCompilationResult result =
//            compiler.CompileGlslToSpv(data, kind, name.c_str(), shaderOut.entry.c_str(), options);
//        if (result.GetCompilationStatus() == shaderc_compilation_status_success) {
//            shaderOut.data.assign(result.begin(), result.end());
//        } else {
//            LOG_E(TAG, "compile error shader:%s, message:\n%s", name.c_str(), result.GetErrorMessage().c_str());
//        }
//
//    }
//
//    bool ShaderBuilder::ParseShader(std::filesystem::path path, const std::string& tag, Document& document,
//        ShaderSourceData& data)
//    {
//        if(!document.HasMember(tag.data())) {
//            return false;
//        }
//        ShaderData shader;
//        shaderc_shader_kind kind;
//
//        auto& val = document[tag.data()];
//        if (tag == "vert") {
//            shader.stage = VK_SHADER_STAGE_VERTEX_BIT;
//            kind = shaderc_vertex_shader;
//        } else if (tag == "frag") {
//            shader.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
//            kind = shaderc_fragment_shader;
//        } else if (tag == "comp") {
//            shader.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//            kind = shaderc_compute_shader;
//        } else {
//            return false;
//        }
//
//        if (!val.HasMember("path")) {
//            return false;
//        }
//        if (val.HasMember("entry")) {
//            shader.entry = val["entry"].GetString();
//        }
//
//        path.append(val["path"].GetString());
//        std::string source;
//        if (!ReadString(path.string(), source)) {
//            return false;
//        }
//
//        CompileShader(path.filename().string(), kind, source, shader, options);
//
//        data.shaders.emplace_back(std::move(shader));
//        return true;
//    }

    ShaderBuilder::ShaderBuilder()
    {
        options.SetIncluder(std::make_unique<ShaderIncluder>());
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

//        ShaderSourceData sourceData;
//        ParseShader(parentPath, "vert", document, sourceData);
//        ParseShader(parentPath, "frag", document, sourceData);
        return true;
    }

    bool ShaderBuilder::Support(const std::string& ext) const
    {
        return ext == PRG_EXT;
    }
}