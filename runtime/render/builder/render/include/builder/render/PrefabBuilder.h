//
// Created by Zach Lee on 2023/2/20.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <string_view>

namespace sky::builder {

    class PrefabBuilder : public AssetBuilder {
    public:
        PrefabBuilder() = default;
        ~PrefabBuilder() override = default;

        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return ""; }
    private:
        std::vector<std::string> extensions = {".gltf", ".glb", ".fbx"};
    };

} // namespace sky::builder
