//
// Created on 2026/09/22.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::builder {

    // Builds a `.terrain` asset from a reflected TerrainSourceData: generates the tile LOD chains.
    class TerrainBuilder : public AssetBuilder {
    public:
        TerrainBuilder()           = default;
        ~TerrainBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;
        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override;

        std::vector<std::string> extensions = {".terrain"};
    };

} // namespace sky::builder
