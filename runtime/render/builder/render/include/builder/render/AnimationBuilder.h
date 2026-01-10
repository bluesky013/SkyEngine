//
// Created by blues on 2024/8/5.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/AnimationAsset.h>

namespace sky::builder {

    class AnimationBuilder : public AssetBuilder {
    public:
        AnimationBuilder() = default;
        ~AnimationBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override
        {
            return ext == ".clip" ? AssetTraits<AnimationClip>::ASSET_TYPE : AssetTraits<Animation>::ASSET_TYPE;
        }

        std::vector<std::string> extensions = {".clip", ".graph"};
    };

} // namespace sky::builder