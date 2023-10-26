//
// Created by Zach Lee on 2023/2/23.
//
#include <render/adaptor/assets/TechniqueAsset.h>

namespace sky {

    void TechniqueAssetData::Load(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);

        shaders.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string idStr;
            archive.LoadValue(idStr);
            shaders[i] = AssetManager::Get()->LoadAsset<Shader>(Uuid::CreateFromString(idStr));
        }

        archive.LoadValue(depthStencil.depthWrite);
        archive.LoadValue(depthStencil.depthTest);
        archive.LoadValue(depthStencil.stencilTest);

        archive.LoadValue(rasterState.cullMode);
        archive.LoadValue(rasterState.frontFace);
        archive.LoadValue(rasterState.polygonMode);

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

        archive.LoadValue(passTag);
        archive.LoadValue(vertexDesc);
    }

    void TechniqueAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(shaders.size()));
        for (const auto &shader : shaders) {
            archive.SaveValue(shader ? shader->GetUuid().ToString() : Uuid{}.ToString());
        }

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

        archive.SaveValue(passTag);
        archive.SaveValue(vertexDesc);
    }

    std::shared_ptr<Technique> CreateTechnique(const TechniqueAssetData &data)
    {
        if (data.shaders.size() > 1) {
            auto tech = std::make_shared<GraphicsTechnique>();
            for (const auto &shader : data.shaders) {
                tech->AddShader(shader->CreateInstance());
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
}
