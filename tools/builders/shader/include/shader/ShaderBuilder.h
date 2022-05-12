//
// Created by Zach Lee on 2022/1/31.
//

#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <shaderc/shaderc.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <framework/interface/IBuilder.h>


namespace sky {

    class ShaderBuilder : public IBuilder {
    public:
        ShaderBuilder();
        ~ShaderBuilder();

        bool Build(const BuildRequest& request) override;

        bool Support(const std::string& ext) const override;

    private:
//        bool ParseShader(std::filesystem::path path, const std::string& tag, rapidjson::Document& document,
//            ShaderSourceData& data);
//
//        void CompileShader(const std::string& name, shaderc_shader_kind kind, const std::string& data,
//            ShaderData& shaderOut, const shaderc::CompileOptions& options);

        friend class ShaderModule;
        std::vector<std::filesystem::path> includePath;
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
    };
}