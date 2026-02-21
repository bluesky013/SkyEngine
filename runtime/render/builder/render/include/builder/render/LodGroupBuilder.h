//
// Created by blues on 2026/2/17.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/LodGroupAsset.h>
#include <memory>

namespace sky::builder {

    class LodGroupBuilder : public AssetBuilder {
    public:
        LodGroupBuilder() = default;
        ~LodGroupBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }

        std::string_view QueryType(const std::string &ext) const override
        {
            return AssetTraits<LodGroup>::ASSET_TYPE;
        }

        std::vector<std::string> extensions = {".lodgroup"};
    };


} // namespace sky::builder
