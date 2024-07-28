//
// Created by Zach Lee on 2023/2/23.
//
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/adaptor/assets/ShaderAsset.h>
#include <shader/ShaderCompiler.h>

namespace sky {

    void TechniqueAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(shader.shader.word[0]);
        archive.LoadValue(shader.shader.word[1]);
        archive.LoadValue(shader.objectOrCSMain);
        archive.LoadValue(shader.vertOrMeshMain);
        archive.LoadValue(shader.fragmentMain);

        archive.LoadValue(passTag);
        archive.LoadValue(vertexDesc);
        archive.LoadValue(type);

        archive.LoadValue(depthStencil.depthWrite);
        archive.LoadValue(depthStencil.depthTest);
        archive.LoadValue(depthStencil.stencilTest);

        archive.LoadValue(rasterState.cullMode);
        archive.LoadValue(rasterState.frontFace);
        archive.LoadValue(rasterState.polygonMode);

        uint32_t size = 0;
        archive.LoadValue(size);
        blendStates.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &blend = blendStates[i];
            archive.LoadValue(blend.blendEn);
            archive.LoadValue(blend.writeMask);
            archive.LoadValue(blend.srcColor);
            archive.LoadValue(blend.dstColor);
            archive.LoadValue(blend.srcAlpha);
            archive.LoadValue(blend.dstAlpha);
            archive.LoadValue(blend.colorBlendOp);
            archive.LoadValue(blend.alphaBlendOp);
        }
    }

    void TechniqueAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(shader.shader.word[0]);
        archive.SaveValue(shader.shader.word[1]);
        archive.SaveValue(shader.objectOrCSMain);
        archive.SaveValue(shader.vertOrMeshMain);
        archive.SaveValue(shader.fragmentMain);
        archive.SaveValue(passTag);
        archive.SaveValue(vertexDesc);
        archive.SaveValue(type);

        archive.SaveValue(depthStencil.depthWrite);
        archive.SaveValue(depthStencil.depthTest);
        archive.SaveValue(depthStencil.stencilTest);

        archive.SaveValue(rasterState.cullMode);
        archive.SaveValue(rasterState.frontFace);
        archive.SaveValue(rasterState.polygonMode);

        archive.SaveValue(static_cast<uint32_t>(blendStates.size()));
        for (const auto &blend : blendStates) {
            archive.SaveValue(blend.blendEn);
            archive.SaveValue(blend.writeMask);
            archive.SaveValue(blend.srcColor);
            archive.SaveValue(blend.dstColor);
            archive.SaveValue(blend.srcAlpha);
            archive.SaveValue(blend.dstAlpha);
            archive.SaveValue(blend.colorBlendOp);
            archive.SaveValue(blend.alphaBlendOp);
        }
    }

    CounterPtr<Technique> CreateTechniqueFromAsset(const TechniqueAssetPtr &asset)
    {
        auto &data = asset->Data();

        if (data.type == TechAssetType::GRAPHIC) {
            auto *tech = new GraphicsTechnique();

            auto shaderAsset = AssetManager::Get()->FindAsset<ShaderCollection>(data.shader.shader);
            if (shaderAsset) {
                auto shader = CreateShaderFromAsset(shaderAsset);
                tech->SetShader({shader, data.shader.objectOrCSMain, data.shader.vertOrMeshMain, data.shader.fragmentMain});
            }

            tech->SetDepthStencil(data.depthStencil);
            tech->SetBlendState(data.blendStates);
            tech->SetRasterState(data.rasterState);
            tech->SetRasterTag(data.passTag);
            tech->SetVertexLayout(data.vertexDesc);

            return tech;
        }
        return nullptr;
    }

    CounterPtr<GraphicsTechnique> GreateGfxTechFromAsset(const TechniqueAssetPtr &asset)
    {
        auto tech = CreateTechniqueFromAsset(asset);
        return static_cast<GraphicsTechnique*>(tech.Get());
    }
}
