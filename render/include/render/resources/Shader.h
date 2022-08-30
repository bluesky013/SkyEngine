//
// Created by Zach Lee on 2022/5/7.
//

#pragma once

#include <render/resources/DescirptorGroup.h>
#include <render/resources/DescriptorPool.h>
#include <render/resources/RenderResource.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/Shader.h>

#include <string>

namespace sky {

    struct StageInputInfo {
        uint32_t location = 0;
        uint32_t size     = 16;
        VkFormat format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    };

    class Shader : public RenderResource {
    public:
        struct Descriptor {
            VkShaderStageFlagBits stage;
        };

        Shader(const Descriptor &desc);
        ~Shader() = default;

        using DescriptorTable = std::unordered_map<uint32_t, drv::DescriptorSetLayout::Descriptor>;
        using NameTable       = std::unordered_map<uint32_t, PropertyTablePtr>;
        using StageInputTable = std::map<std::string, StageInputInfo>;

        void LoadData(const std::string &path);

        void SetData(std::vector<uint32_t> &&data);

        void InitRHI();

        bool IsValid() const override;

        drv::ShaderPtr GetShader() const;

        const DescriptorTable &GetDescriptorTable() const;

        const StageInputTable &GetStageInputs() const;

        const NameTable &GetNameTable() const;

        uint32_t GetConstantBlockSize() const;

    private:
        void BuildReflection();

        Descriptor            descriptor;
        std::vector<uint32_t> spv;
        drv::ShaderPtr        rhiShader;
        StageInputTable       stageInputs;
        NameTable             nameTable;
        DescriptorTable       descriptorTable;
        uint32_t              constantBlockSize = 0;
    };
    using RDShaderPtr = std::shared_ptr<Shader>;

    class ShaderTable : public RenderResource {
    public:
        ShaderTable()  = default;
        ~ShaderTable() = default;

        void InitRHI();

        drv::PipelineLayoutPtr GetPipelineLayout() const;

        RDDesGroupPtr CreateDescriptorGroup(uint32_t slot) const;

        void FillProgram(drv::GraphicsPipeline::Program &program);

    protected:
        std::vector<RDShaderPtr> shaders;
        Shader::NameTable        nameTable;
        drv::PipelineLayoutPtr   pipelineLayout;
    };
    using RDShaderTablePtr = std::shared_ptr<ShaderTable>;

    class ComputeShaderTable : public ShaderTable {
    public:
        ComputeShaderTable()  = default;
        ~ComputeShaderTable() = default;
    };
    using RDCompShaderTable = std::shared_ptr<ComputeShaderTable>;

    class RayTracingShaderTable : public ShaderTable {
    public:
        RayTracingShaderTable()  = default;
        ~RayTracingShaderTable() = default;
    };
    using RDRTShaderTable = std::shared_ptr<RayTracingShaderTable>;

    class GraphicsShaderTable : public ShaderTable {
    public:
        GraphicsShaderTable()  = default;
        ~GraphicsShaderTable() = default;

        void LoadShader(const std::string &vs, const std::string &fs);

        bool IsValid() const override;

        inline RDShaderPtr GetVS() const
        {
            return vs;
        }

        inline RDShaderPtr GetFS() const
        {
            return fs;
        }

    private:
        RDShaderPtr vs;
        RDShaderPtr fs;
    };
    using RDGfxShaderTablePtr = std::shared_ptr<GraphicsShaderTable>;

} // namespace sky