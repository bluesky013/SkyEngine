//
// Created by blues on 2026/9/29.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <string_view>

namespace sky::builder {

    // OpenUSD (.usd/.usda/.usdc/.usdz) 导入器，产出与 PrefabBuilder 同构的源资产
    // 需要 SKY_WITH_USD（3rd/output/usd 存在时由 cmake 定义），否则退化为空实现
    class UsdBuilder : public AssetBuilder {
    public:
        UsdBuilder() = default;
        ~UsdBuilder() override = default;

        void Import(const AssetImportRequest &request) const override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return AssetTraits<RenderPrefab>::ASSET_TYPE; }
    private:
        std::vector<std::string> extensions = {".usd", ".usda", ".usdc", ".usdz"};
    };

} // namespace sky::builder
