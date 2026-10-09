//
// Aurora skeleton builder: cooks a `.skeleton` JSON source into a
// SkeletonAssetData (bone hierarchy + ref pose).
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class AuroraSkeletonBuilder : public AssetBuilder {
    public:
        AuroraSkeletonBuilder()           = default;
        ~AuroraSkeletonBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &ext) const override;
        void             Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        std::vector<std::string> extensions = {".skeleton"};
    };

} // namespace sky::aurora
