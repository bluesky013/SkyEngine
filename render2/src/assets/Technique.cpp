//
// Created by Zach Lee on 2023/2/23.
//
#include <render/assets/Technique.h>

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
    }

    void TechniqueAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(shaders.size()));
        for (auto &shader : shaders) {
            archive.SaveValue(shader ? shader->GetUuid() : Uuid{});
        }

        archive.SaveValue(depthStencil.depthWrite);
        archive.SaveValue(depthStencil.depthTest);
        archive.SaveValue(depthStencil.stencilTest);
    }


}