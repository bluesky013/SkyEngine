//
// Created by blues on 2024/7/14.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky::builder {
    class MeshBuilder : public AssetBuilder {
    public:
        MeshBuilder() = default;
        ~MeshBuilder() override = default;

    private:
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }

        std::string_view QueryType(const std::string &ext) const override
        {
            return AssetTraits<Mesh>::ASSET_TYPE;
        }

        std::vector<std::string> extensions = {".mesh"};
    };

} // namespace sky::builder
