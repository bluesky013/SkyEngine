//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <render/VertexDescLibrary.h>
#include <rhi/Core.h>
#include <string>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct VertexDescLibraryAssetData {
        std::unordered_map<std::string, rhi::VertexInput::Descriptor> descriptions;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    VertexDescLibrary *CreateVertexDescLibrary(const VertexDescLibraryAssetData &data);

    template <>
    struct AssetTraits<VertexDescLibrary> {
        using DataType                                = VertexDescLibraryAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("f344d674-c0b1-4e1d-b6c5-87aba4aea204");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using VertexDescLibraryAssetPtr = std::shared_ptr<Asset<VertexDescLibrary>>;
} // namespace sky
