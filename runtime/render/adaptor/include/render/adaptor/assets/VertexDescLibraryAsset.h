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
        static constexpr std::string_view ASSET_TYPE  = "VertexDescLibrary";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using VertexDescLibraryAssetPtr = std::shared_ptr<Asset<VertexDescLibrary>>;
} // namespace sky
