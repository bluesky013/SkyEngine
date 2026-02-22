//
// Created by blues on 2024/8/10.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/SkeletonAsset.h>

namespace sky::builder {

    class SkeletonBuilder : public AssetBuilder {
    public:
        SkeletonBuilder() = default;
        ~SkeletonBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return AssetTraits<Skeleton>::ASSET_TYPE; }

        std::vector<std::string> extensions = {".skeleton"};
    };

} // namespace sky::builder