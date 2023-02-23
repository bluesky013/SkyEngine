//
// Created by Zach Lee on 2023/2/23.
//
#include <render/assets/Technique.h>

namespace sky {

    void TechniqueAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(depthStencil.depthWrite);
        archive.LoadValue(depthStencil.depthTest);
        archive.LoadValue(depthStencil.stencilTest);
    }

    void TechniqueAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(depthStencil.depthWrite);
        archive.SaveValue(depthStencil.depthTest);
        archive.SaveValue(depthStencil.stencilTest);
    }


}