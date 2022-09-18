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
    void serialize(Archive &ar, sky::drv::GraphicsPipeline::DepthStencilState &s)
    {
        ar(s.depthTestEnable, s.depthWriteEnable, s.depthCompareOp, s.depthBoundsTestEnable, s.stencilTestEnable, s.front, s.back, s.minDepthBounds,
           s.maxDepthBounds);
    }

    template <class Archive>
    void serialize(Archive &ar, sky::drv::GraphicsPipeline::Raster &s)
    {
        ar(s.depthClampEnable, s.rasterizerDiscardEnable, s.polygonMode, s.cullMode, s.frontFace, s.depthBiasEnable, s.depthBiasConstantFactor,
           s.depthBiasClamp, s.depthBiasSlopeFactor, s.lineWidth);
    }

    template <class Archive>
    void serialize(Archive &ar, sky::drv::GraphicsPipeline::BlendState &s)
    {
        ar(s.blendEnable, s.srcColorBlendFactor, s.dstColorBlendFactor, s.colorBlendOp, s.srcAlphaBlendFactor, s.dstAlphaBlendFactor, s.alphaBlendOp,
           s.colorWriteMask);
    }

    template <class Archive>
    void serialize(Archive &ar, sky::drv::GraphicsPipeline::ColorBlend &s)
    {
        ar(s.blendStates);
    }

    template <class Archive>
    struct specialize<Archive, VkStencilOpState, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::drv::GraphicsPipeline::BlendState, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::drv::GraphicsPipeline::ColorBlend, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::drv::GraphicsPipeline::DepthStencilState, cereal::specialization::non_member_serialize> {
    };

    template <class Archive>
    struct specialize<Archive, sky::drv::GraphicsPipeline::Raster, cereal::specialization::non_member_serialize> {
    };
}

namespace sky {

    struct GfxTechniqueAssetData {
        ShaderAssetPtr vs;
        ShaderAssetPtr fs;
        drv::GraphicsPipeline::DepthStencilState depthStencilState;
        drv::GraphicsPipeline::Raster raster;
        drv::GraphicsPipeline::ColorBlend blends;
        std::string rasterTag;
        std::unordered_map<Uuid, std::string> assetPathMap;

        template <class Archive>
        void save(Archive &ar) const
        {
            ar(vs->GetUuid(), fs->GetUuid(), depthStencilState, raster, blends, assetPathMap);
        }

        template <class Archive>
        void load(Archive &ar)
        {
            Uuid vsId;
            Uuid fsId;
            ar(vsId, fsId, depthStencilState, raster, blends, assetPathMap);

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

        drv::GraphicsPipelinePtr AcquirePso(const drv::VertexInputPtr &vertexInput);

        drv::GraphicsPipelinePtr AcquirePso(const drv::VertexInputPtr &vi, const drv::ShaderOptionPtr &option);

        void SetViewTag(uint32_t tag);

        void SetDrawTag(uint32_t tag);

        uint32_t GetViewTag() const;

        uint32_t GetDrawTag() const;

        RDGfxShaderTablePtr GetShaderTable() const;

        drv::DescriptorSetBinderPtr CreateSetBinder() const;

        void SetDepthTestEn(bool enable);

        void SetDepthWriteEn(bool enable);

        void SetDepthStencilState(const drv::GraphicsPipeline::DepthStencilState &ds);

        void SetBlendState(const drv::GraphicsPipeline::ColorBlend &blends);

        void SetRasterState(const drv::GraphicsPipeline::Raster &raster);

        drv::GraphicsPipeline::State &GetState();

        static std::shared_ptr<GraphicsTechnique> CreateFromData(const GfxTechniqueAssetData& data);

    private:
        RDGfxShaderTablePtr                                    table;
        uint32_t                                               subPassIndex = 0;
        uint32_t                                               viewTag      = 0;
        uint32_t                                               drawTag      = 0;
        RDPassPtr                                              pass;
        drv::GraphicsPipeline::State                           pipelineState;
        std::unordered_map<uint32_t, drv::GraphicsPipelinePtr> psoCache;
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