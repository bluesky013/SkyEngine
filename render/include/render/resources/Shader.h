//
// Created by Zach Lee on 2022/5/7.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <vulkan/Shader.h>
#include <vulkan/PipelineLayout.h>

#include <string>

namespace sky {

    class Shader : public RenderResource {
    public:
        struct Descriptor {
            VkShaderStageFlagBits stage;
        };

        Shader(const Descriptor& desc);
        ~Shader() = default;

        void LoadData(const std::string& path);

        void SetData(std::vector<uint32_t>&& data);

        void InitRHI() override;

        bool IsValid() const override;

        using DescriptorTable = std::unordered_map<uint32_t, drv::DescriptorSetLayout::Descriptor>;
        const DescriptorTable& GetDescriptorTable() const;

    private:
        void BuildReflection();

        Descriptor descriptor;
        std::vector<uint32_t> spv;
        drv::ShaderPtr rhiShader;

        DescriptorTable descriptorTable;
    };
    using RDShaderPtr = std::shared_ptr<Shader>;

    class ShaderTable : public RenderResource {
    public:
        ShaderTable() = default;
        ~ShaderTable() = default;

        void InitRHI() override;

        drv::PipelineLayoutPtr GetPipelineLayout() const;

    protected:
        std::vector<RDShaderPtr> shaders;
        drv::PipelineLayoutPtr pipelineLayout;
    };
    using RDShaderTablePtr = std::shared_ptr<ShaderTable>;

    class ComputeShaderTable : public ShaderTable {
    public:
        ComputeShaderTable() = default;
        ~ComputeShaderTable() = default;
    };
    using RDCompShaderTable = std::shared_ptr<ComputeShaderTable>;

    class RayTracingShaderTable : public ShaderTable {
    public:
        RayTracingShaderTable() = default;
        ~RayTracingShaderTable() = default;
    };
    using RDRTShaderTable = std::shared_ptr<RayTracingShaderTable>;

    class GraphicsShaderTable : public ShaderTable {
    public:
        GraphicsShaderTable() = default;
        ~GraphicsShaderTable() = default;

        void LoadShader(const std::string& vs, const std::string& fs);

        bool IsValid() const override;

    private:
        RDShaderPtr vs;
        RDShaderPtr fs;
    };
    using RDGfxShaderTablePtr = std::shared_ptr<GraphicsShaderTable>;

}