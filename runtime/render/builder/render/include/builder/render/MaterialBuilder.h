//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <string_view>

namespace sky::builder {

    class MaterialBuilder : public AssetBuilder {
    public:
        MaterialBuilder() = default;
        ~MaterialBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }

        std::string_view QueryType(const std::string &ext) const override
        {
            return ext == ".mat" ? AssetTraits<Material>::ASSET_TYPE : AssetTraits<MaterialInstance>::ASSET_TYPE;
        }

        static void BuildMaterial(const AssetBuildRequest &request, AssetBuildResult &result);
        static void BuildMaterialInstance(const AssetBuildRequest &request, AssetBuildResult &result);

        std::vector<std::string> extensions = {".mat", ".mati"};
    };

}
