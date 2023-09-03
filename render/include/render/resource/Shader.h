//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <vector>
#include <memory>
#include <rhi/Device.h>

namespace sky {

    class ShaderVariant {
    public:
        ShaderVariant() = default;
        ~ShaderVariant() = default;

        bool Init(rhi::ShaderStageFlagBit stage, const uint8_t *data, uint32_t size);

    private:
        rhi::ShaderPtr shader;
    };
    using ShaderVariantPtr = std::shared_ptr<ShaderVariant>;

    class Shader {
    public:
        Shader() = default;
        ~Shader() = default;

        void AddVariant(const std::string &key, const ShaderVariantPtr &variant);
    private:
        std::unordered_map<std::string, ShaderVariantPtr> variants;
    };
    using RDShaderPtr = std::shared_ptr<Shader>;

    class Program {
    public:
        Program() = default;
        ~Program() = default;

    private:
        std::vector<rhi::ShaderPtr> shaders;
        rhi::PipelineLayoutPtr pipelineLayout;
    };
    using RDProgramPtr = std::shared_ptr<Program>;

} // namespace sky
