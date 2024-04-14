//
// Created by Zach Lee on 2023/2/20.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <string_view>

namespace sky::builder {

    class PrefabBuilder : public AssetBuilder {
    public:
        PrefabBuilder() = default;
        ~PrefabBuilder() override = default;

        static constexpr std::string_view KEY = "GFX_PREFAB";

        void Request(const BuildRequest &build, BuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }

    private:
        std::vector<std::string> extensions = {".gltf", ".glb", ".fbx"};
    };

} // namespace sky::builder
