//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/VertexDescLibraryAsset.h>
#include <string_view>

namespace sky::builder {
    class VertexLibraryBuilder : public AssetBuilder {
    public:
        VertexLibraryBuilder() = default;
        ~VertexLibraryBuilder() override = default;

        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;
        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return AssetTraits<VertexDescLibrary>::ASSET_TYPE; }
    private:
        std::vector<std::string> extensions = {".vtxlib"};
    };
} // sky::builder
