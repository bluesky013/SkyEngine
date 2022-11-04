//
// Created by Zach Lee on 2022/5/28.
//

#pragma once

#include <render/resources/Pass.h>
#include <render/resources/RenderResource.h>
#include <render/resources/Shader.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/GraphicsPipeline.h>

namespace cereal {
    template <class Archive>
    void serialize(Archive &ar, VkStencilOpState &s)
    {
        ar(s.failOp, s.passOp, s.depthFailOp, s.compareOp, s.compareMask, s.writeMask, s.reference);
    }

    template <class Archive>
    void serialize(Archive &ar, sky::vk::GraphicsPipeline::DepthStencilState &s)
    {
        ar(s.depthTestEnable, s.depthWriteEnable, s.depthCompareOp, s.depthBoundsTestEnable, s.stencilTestEnable, s.front, s.back, s.minDepthBounds,
           s.maxDepthBounds);
    }

    template <class Archive>
    void serialize(Archive &ar, sky::vk::GraphicsPipeline::Raster &s)
    {
        ar(s.depthClampEnable, s.rasterizerDiscardEnable, s.polygonMode, s.cullMode, s.frontFace, s.depthBiasEnable, s.depthBiasConstantFactor,
           s.depthBiasClamp, s.depthBiasSlopeFactor, s.lineWidth);
    }

    template <class Archive>
    void serialize(Archive &ar, sky::vk::GraphicsPipeline::BlendState &s)
    {
        ar(s.blendEnable, s.srcColorBlendFactor, s.dstColorBlendFactor, s.colorBlendOp, s.srcAlphaBlendFactor, s.dstAlphaBlendFactor, s.alphaBlendOp,
           s.colorWriteMask);
    }

    template <class Archive>
    void serialize(Archive &ar, sky::vk::GraphicsPipeline::ColorBlend &s)
    {
        ar(s.blendStates);
    }

    template <class Archive>
    struct specialize<Archive, VkStencilOpState, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::vk::GraphicsPipeline::BlendState, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::vk::GraphicsPipeline::ColorBlend, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::vk::GraphicsPipeline::DepthStencilState, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::vk::GraphicsPipeline::Raster, cereal::specialization::non_member_serialize> {
    };
}

namespace sky {

    struct GfxTechniqueAssetData {
        ShaderAssetPtr vs;
        ShaderAssetPtr fs;
        vk::GraphicsPipeline::DepthStencilState depthStencilState;
        vk::GraphicsPipeline::Raster raster;
        vk::GraphicsPipeline::ColorBlend blends;
        uint32_t viewTag;
        uint32_t drawTag;
        std::unordered_map<Uuid, std::string> assetPathMap;

        template <class Archive>
        void save(Archive &ar) const
        {
            ar(vs->GetUuid(), fs->GetUuid(), depthStencilState, raster, blends, viewTag, drawTag, assetPathMap);
        }

        template <class Archive>
        void load(Archive &ar)
        {
            Uuid vsId;
            Uuid fsId;
            ar(vsId, fsId, depthStencilState, raster, blends, viewTag, drawTag, assetPathMap);

            InitShader(vsId, vs);
            InitShader(fsId, fs);
        }

        void InitShader(const Uuid &id, ShaderAssetPtr &asset);
    };

    class GraphicsTechnique : public RenderResource {
    public:
        GraphicsTechnique()  = default;
        ~GraphicsTechnique() override = default;

        void SetShaderTable(const RDGfxShaderTablePtr &table);

        void SetRenderPass(const RDPassPtr &pass, uint32_t subPass = 0);

        vk::GraphicsPipelinePtr AcquirePso(const vk::VertexInputPtr &vertexInput);

        vk::GraphicsPipelinePtr AcquirePso(const vk::VertexInputPtr &vi, const vk::ShaderOptionPtr &option);

        void SetViewTag(uint32_t tag);

        void SetDrawTag(uint32_t tag);

        uint32_t GetViewTag() const;

        uint32_t GetDrawTag() const;

        const RDGfxShaderTablePtr &GetShaderTable() const;

        vk::DescriptorSetBinderPtr CreateSetBinder() const;

        void SetDepthTestEn(bool enable);

        void SetDepthWriteEn(bool enable);

        void SetDepthStencilState(const vk::GraphicsPipeline::DepthStencilState &ds);

        void SetBlendState(const vk::GraphicsPipeline::ColorBlend &blends);

        void SetRasterState(const vk::GraphicsPipeline::Raster &raster);

        vk::GraphicsPipeline::State &GetState();

        static std::shared_ptr<GraphicsTechnique> CreateFromData(const GfxTechniqueAssetData& data);

    private:
        RDGfxShaderTablePtr                                    table;
        uint32_t                                               subPassIndex = 0;
        uint32_t                                               viewTag      = 0;
        uint32_t                                               drawTag      = 0;
        RDPassPtr                                              pass;
        vk::GraphicsPipeline::State                           pipelineState;
        std::unordered_map<uint32_t, vk::GraphicsPipelinePtr> psoCache;
    };
    using RDGfxTechniquePtr = std::shared_ptr<GraphicsTechnique>;

    template <>
    struct AssetTraits<GraphicsTechnique> {
        using DataType                                = GfxTechniqueAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("79F513A7-8BC1-48B4-B086-FB2E78798D60");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static RDGfxTechniquePtr CreateFromData(const DataType &data)
        {
            return GraphicsTechnique::CreateFromData(data);
        }
    };
    using GfxTechniqueAssetPtr = std::shared_ptr<Asset<GraphicsTechnique>>;

} // namespace sky
