//
// Created by blues on 2024/6/16.
//

#pragma once

#include <vector>
#include <string>
#include <utility>
#include <type_traits>
#include <core/file/FileSystem.h>
#include <core/util/Uuid.h>
#include <core/template/ReferenceObject.h>
#include <core/event/Event.h>
#include <framework/serialization/Any.h>

namespace sky {

    // asset source info
    struct AssetSourceInfo : public RefObject {
        FilePath path;                  // logical source path within the mounted namespace
        std::string name;               // marked name used to load by name, can be empty
        std::string ext;                // file extension (source of the derived AssetTypeId)
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
        FilePath savePath;
        Any config;
    };

    using ProductBundleKey = std::string;
    struct AssetBuildRequest {
        FilePtr file;
        AssetSourcePtr assetInfo;
        ProductBundleKey target;
    };

    struct AssetBuildResult {
        Uuid uuid;
        ProductBundleKey target;
        AssetBuildRetCode retCode;
        std::string error;
    };

    struct AssetRawData {
        std::vector<uint8_t> storage;
    };
} // namespace sky

