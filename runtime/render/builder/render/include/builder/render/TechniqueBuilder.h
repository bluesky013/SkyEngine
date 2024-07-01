//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <string_view>
#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/TechniqueAsset.h>

namespace sky::builder {

    class TechniqueBuilder : public AssetBuilder {
    public:
        TechniqueBuilder() = default;
        ~TechniqueBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;
        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return AssetTraits<Technique>::ASSET_TYPE; }

        std::vector<std::string> extensions = {".tech"};
    };

}
