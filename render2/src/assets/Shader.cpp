//
// Created by Zach Lee on 2023/2/23.
//

#include <render/assets/Shader.h>
#include <framework/serialization/BinaryArchive.h>

namespace sky {

    void ShaderAssetData::Load(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);

        data.resize(size / sizeof(uint32_t));
        archive.LoadValue(reinterpret_cast<char *>(data.data()), size);
    }

    void ShaderAssetData::Save(BinaryOutputArchive &archive) const
    {
        uint32_t size = static_cast<uint32_t>(data.size() * sizeof(uint32_t));
        archive.SaveValue(size);
        archive.SaveValue(reinterpret_cast<const char*>(data.data()), size);
    }

}