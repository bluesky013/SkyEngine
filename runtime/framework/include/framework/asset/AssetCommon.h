//
// Created by blues on 2024/6/16.
//

#pragma once

#include <vector>
#include <string>
#include <utility>
#include <type_traits>
#include <core/file/FileSystem.h>
#include <core/hash/Hash.h>
#include <core/hash/Fnv1a.h>
#include <core/util/Uuid.h>
#include <core/template/ReferenceObject.h>
#include <core/event/Event.h>

namespace sky {

    enum class SourceAssetBundle : uint32_t {
        INVALID,
        ENGINE,
        WORKSPACE,
        CUSTOM_BEGIN = WORKSPACE + 1
    };

    struct AssetSourcePath {
        SourceAssetBundle bundle;       // asset bundle
        FilePath path;                  // relative path to bundle
    };

    // asset source info
    struct AssetSourceInfo : public RefObject {
        AssetSourcePath path;           // asset path
        std::string name;               // marked name used to load by name, can be empty
        std::string ext;                // file extension
        std::string category;           // asset category
        Uuid uuid;                      // uuid of the asset
        std::vector<Uuid> dependencies; // dependent assets
    };
    using AssetSourcePtr = CounterPtr<AssetSourceInfo>;

    enum class AssetBuildRetCode : uint32_t {
        SUCCESS,
        FAILED,
    };

    struct AssetImportRequest {
        FilePath filePath;
    };

    using ProductBundleKey = std::string;
    struct AssetBuildRequest {
        FilePtr file;
        AssetSourcePtr assetInfo;
        ProductBundleKey target;
    };

    struct AssetBuildResult {
        AssetBuildRetCode retCode;
    };

    struct AssetRawData {
        std::vector<uint8_t> storage;
    };
} // namespace sky

namespace std {

    template <>
    struct hash<sky::AssetSourcePath> {
        size_t operator()(const sky::AssetSourcePath &path) const noexcept
        {
            auto hash = static_cast<uint32_t>(path.bundle);
            sky::HashCombine32(hash, sky::Fnv1a32(path.path.GetStr()));
            return hash;
        }
    };

    template <>
    struct equal_to<sky::AssetSourcePath> {
        bool operator()(const sky::AssetSourcePath &x, const sky::AssetSourcePath &y) const noexcept
        {
            return x.bundle == y.bundle && x.path == y.path;
        }
    };

} // namespace std