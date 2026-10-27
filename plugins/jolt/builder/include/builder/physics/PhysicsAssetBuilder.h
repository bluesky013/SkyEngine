#pragma once

#include <framework/asset/AssetBuilder.h>

namespace sky::phy {
    class PhysicsAssetBuilder final : public AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override;
        std::string_view                QueryType(const std::string &extension) const override;
        void                            Request(const AssetBuildRequest &request, AssetBuildResult &result) override;
    };
} // namespace sky::phy
