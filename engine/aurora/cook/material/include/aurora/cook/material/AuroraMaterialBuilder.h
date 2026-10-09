//
// Aurora material builder: cooks a `.mat` JSON source into MaterialAssetData
// (inline techniques + typed property defaults).
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class AuroraMaterialBuilder : public AssetBuilder {
    public:
        AuroraMaterialBuilder()           = default;
        ~AuroraMaterialBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &ext) const override;
        void             Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        std::vector<std::string> extensions = {".mat"};
    };

} // namespace sky::aurora
