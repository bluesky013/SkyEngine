//
// Created by Zach Lee on 2023/2/20.
//

#pragma once
#include <string>
#include <vector>
#include <core/util/Uuid.h>
#include <core/platform/Platform.h>
#include <core/file/FileSystem.h>
#include <framework/asset/Asset.h>

namespace sky {

    struct BuildProduct {
        std::string productKey;
        std::shared_ptr<AssetBase> asset;
        std::vector<Uuid> deps;
    };

    struct BuildRequest {
        Uuid uuid;
        std::string relativePath;
        std::string fullPath;
        std::string name;
        std::string ext;
        PlatformType targetPlatform;
        std::string buildKey;
        const void *rawData = nullptr;
        uint32_t dataSize = 0;
    };

    struct BuildResult {
        bool success = false;
        std::vector<BuildProduct> products;
    };

    class AssetBuilder {
    public:
        AssetBuilder() = default;
        virtual ~AssetBuilder() = default;

        virtual void Request(const BuildRequest &build, BuildResult &result) = 0;
        virtual const std::vector<std::string> &GetExtensions() const = 0;

        virtual std::string GetConfigKey() const { return ""; }
        virtual void LoadConfig(const FileSystemPtr& fs, const std::string &path) {}
    };

} // namespace sky