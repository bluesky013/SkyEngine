//
// Aurora material-instance builder: cooks a `.mati` JSON source into
// MaterialInstanceAssetData (parent material + value / texture overrides).
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class AuroraMaterialInstanceBuilder : public AssetBuilder {
    public:
        AuroraMaterialInstanceBuilder()           = default;
        ~AuroraMaterialInstanceBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &ext) const override;
        void             Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        std::vector<std::string> extensions = {".mati"};
    };

} // namespace sky::aurora
