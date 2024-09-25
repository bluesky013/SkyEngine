//
// Created by Zach Lee on 2023/2/20.
//

#pragma once
#include <string>
#include <vector>
#include <core/util/Uuid.h>
#include <core/platform/Platform.h>
#include <core/file/FileSystem.h>
#include <core/util/String.h>
#include <framework/asset/Asset.h>
#include <framework/asset/AssetCommon.h>

namespace sky {

    class AssetBuilder {
    public:
        AssetBuilder() = default;
        virtual ~AssetBuilder() = default;

        virtual const std::vector<std::string> &GetExtensions() const = 0;

        virtual void Import(const AssetImportRequest &request) const {}

        virtual void Request(const AssetBuildRequest &request, AssetBuildResult &result) {}

        virtual std::string_view QueryType(const std::string &ext) const { return ""; }

        virtual void LoadConfig(const FileSystemPtr &cfg) {}
    };
} // namespace sky