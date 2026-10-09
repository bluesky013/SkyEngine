//
// Aurora lod group builder: cooks a `.lod` JSON source into LodGroupAssetData
// (ordered levels binding a screen size to a mesh asset).
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class AuroraLodGroupBuilder : public AssetBuilder {
    public:
        AuroraLodGroupBuilder()           = default;
        ~AuroraLodGroupBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &ext) const override;
        void             Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        std::vector<std::string> extensions = {".lod"};
    };

} // namespace sky::aurora
