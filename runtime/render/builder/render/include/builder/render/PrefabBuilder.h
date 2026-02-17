//
// Created by Zach Lee on 2023/2/20.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/RenderPrefab.h>
#include <string_view>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::builder {

    struct PrefabImportConfig {
        bool skeletonOnly = false;
        bool replaceNameSpace = false;
    };

    class PrefabBuilder : public AssetBuilder {
    public:
        PrefabBuilder() = default;
        ~PrefabBuilder() override = default;

        static void Reflect(SerializationContext* context);

        Any RequireImportSetting(const FilePath &request) const override;
        void Import(const AssetImportRequest &request) const override;
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return ext == ".prefab" ? AssetTraits<RenderPrefab>::ASSET_TYPE : ""; }
    private:
        std::vector<std::string> extensions = {".gltf", ".glb", ".fbx", ".prefab"};
    };

} // namespace sky::builder
