//
// Created on 2026/09/21.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::builder {

    class NaviMeshBuilder : public AssetBuilder {
    public:
        NaviMeshBuilder()           = default;
        ~NaviMeshBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;
        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override;

        std::vector<std::string> extensions = {".navmesh"};
    };

} // namespace sky::builder
