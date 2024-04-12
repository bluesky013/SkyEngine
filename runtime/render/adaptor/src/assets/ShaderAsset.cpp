//
// Created by Zach Lee on 2023/2/23.
//

#include <framework/asset/AssetManager.h>
#include <framework/serialization/BinaryArchive.h>
#include <render/adaptor/assets/ShaderAsset.h>

#include <render/RHI.h>

namespace sky {
    void ShaderAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(shaderSource);
        archive.LoadValue(name);
        archive.LoadValue(hash);
    }

    void ShaderAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(shaderSource);
        archive.SaveValue(name);
        archive.SaveValue(hash);
    }

    std::shared_ptr<ShaderCollection> CreateShader(const ShaderAssetData &data)
    {
        return std::make_shared<ShaderCollection>(data.name, data.shaderSource, data.hash);
    }
}
