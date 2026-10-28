//
// Created on 2026/10/27.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::builder {

    class WorldBuilder final : public AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &ext) const override;
        void             Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        const std::vector<std::string> extensions = {".world"};
    };

} // namespace sky::builder
