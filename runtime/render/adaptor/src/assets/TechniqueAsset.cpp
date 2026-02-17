//
// Created by Zach Lee on 2023/2/23.
//
#include <render/adaptor/assets/TechniqueAsset.h>
#include <shader/ShaderCompiler.h>

namespace sky {

    void TechniqueAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(shader.shader);
        archive.LoadValue(shader.taskOrCSMain);
        archive.LoadValue(shader.vertexMain);
        archive.LoadValue(shader.meshMain);
        archive.LoadValue(shader.fragmentMain);

        archive.LoadValue(passTag);
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

        archive.LoadValue(size);
        vertexFlags.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &flag = vertexFlags[i];
            archive.LoadValue(flag.flag);
            archive.LoadValue(flag.macro);
        }
    }

    void TechniqueAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(shader.shader);
        archive.SaveValue(shader.taskOrCSMain);
        archive.SaveValue(shader.vertexMain);
        archive.SaveValue(shader.meshMain);
        archive.SaveValue(shader.fragmentMain);
        archive.SaveValue(passTag);
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

        archive.SaveValue(static_cast<uint32_t>(vertexFlags.size()));
        for (const auto &flag : vertexFlags) {
            archive.SaveValue(flag.flag);
            archive.SaveValue(flag.macro);
        }
    }

    CounterPtr<Technique> CreateTechniqueFromAsset(const TechniqueAssetPtr &asset)
    {
        auto &data = asset->Data();

        if (data.type == TechAssetType::GRAPHIC) {
            auto *tech = new GraphicsTechnique();

            tech->SetShader({Name(data.shader.shader.c_str()), data.shader.taskOrCSMain, data.shader.vertexMain, data.shader.meshMain, data.shader.fragmentMain});
            tech->SetDepthStencil(data.depthStencil);
            tech->SetBlendState(data.blendStates);
            tech->SetRasterState(data.rasterState);
            tech->SetRasterTag(Name(data.passTag.c_str()));

            for (const auto &[bit, str] : data.vertexFlags) {
                tech->AddVertexFlag(static_cast<RenderVertexFlagBit>(bit.value), Name(str.c_str()));
            }
            return tech;
        }
        return nullptr;
    }

    CounterPtr<GraphicsTechnique> CreateGfxTechFromAsset(const TechniqueAssetPtr &asset)
    {
        auto tech = CreateTechniqueFromAsset(asset);
        return static_cast<GraphicsTechnique*>(tech.Get());
    }
}
