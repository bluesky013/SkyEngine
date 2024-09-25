//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/ShaderAsset.h>
#include <string_view>

namespace sky::builder {

    class ShaderBuilder : public AssetBuilder {
    public:
        ShaderBuilder() = default;
        ~ShaderBuilder() override = default;

        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;
        void LoadConfig(const FileSystemPtr &cfg) override;
        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return AssetTraits<ShaderCollection>::ASSET_TYPE; }
    private:
        std::vector<std::string> extensions = {".hlsl"};
    };

}
