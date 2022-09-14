//
// Created by Zach Lee on 2022/5/28.
//

#pragma once

#include <render/resources/Pass.h>
#include <render/resources/RenderResource.h>
#include <render/resources/Shader.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/GraphicsPipeline.h>

namespace sky {

    struct GfxTechniqueAssetData {
        ShaderAssetPtr vs;
        ShaderAssetPtr fs;

        template <class Archive>
        void save(Archive &ar) const
        {
            ar(vs->GetUuid(), fs->GetUuid());
        }

        template <class Archive>
        void load(Archive &ar)
        {
            Uuid vsId;
            Uuid fsId;
            ar(vsId, fsId);

            InitShader(vsId, vs);
            InitShader(fsId, fs);
        }

        static void InitShader(const Uuid &id, ShaderAssetPtr &asset);
    };

    class GraphicsTechnique : public RenderResource {
    public:
        GraphicsTechnique()  = default;
        ~GraphicsTechnique() = default;

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

        drv::GraphicsPipeline::State &GetState();

        static std::shared_ptr<GraphicsTechnique> CreateFromData(const GfxTechniqueAssetData& data);

    private:
        bool CheckVertexInput(drv::VertexInput &input) const;

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