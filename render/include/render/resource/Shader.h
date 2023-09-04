//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <vector>
#include <memory>
#include <rhi/Device.h>

namespace sky {

    struct ShaderBufferMember {
        std::string name;
        uint32_t offset;
        uint32_t size;
    };

    struct ShaderResource {
        std::string name;
        uint32_t set;
        uint32_t binding;
        rhi::DescriptorType type;
        std::vector<ShaderBufferMember> members;
    };

    class ShaderVariant {
    public:
        ShaderVariant() = default;
        ~ShaderVariant() = default;

        bool Init(rhi::ShaderStageFlagBit stage, const uint8_t *data, uint32_t size);
        void SetShaderResources(const std::vector<ShaderResource> &res);
        const std::vector<ShaderResource> &GetShaderResources() const { return resources; }
        const rhi::ShaderPtr &GetShader() const { return shader; }
        rhi::ShaderStageFlagBit GetStage() const { return stage; }

    private:
        rhi::ShaderPtr shader;
        rhi::ShaderStageFlagBit stage;
        std::vector<ShaderResource> resources;
    };
    using ShaderVariantPtr = std::shared_ptr<ShaderVariant>;

    class Shader {
    public:
        Shader() = default;
        ~Shader() = default;

        void AddVariant(const std::string &key, const ShaderVariantPtr &variant);
        const ShaderVariantPtr &GetVariant(const std::string &key) const;
    private:
        std::unordered_map<std::string, ShaderVariantPtr> variants;
    };
    using RDShaderPtr = std::shared_ptr<Shader>;

    class Program {
    public:
        Program() = default;
        ~Program() = default;

        void AddShader(const ShaderVariantPtr &shader);
        void BuildPipelineLayout();

    private:
        std::vector<ShaderVariantPtr> shaders;
        rhi::PipelineLayoutPtr pipelineLayout;
    };
    using RDProgramPtr = std::shared_ptr<Program>;

} // namespace sky
