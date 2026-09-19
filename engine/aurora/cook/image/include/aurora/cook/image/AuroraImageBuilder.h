//
// Aurora image asset builder. Declares the source extensions it owns and maps
// them to the aurora Texture asset type. The cook pipeline (decode -> resize ->
// mip -> compress -> ImageAssetData) is filled in by later tasks.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

#include <string>
#include <string_view>
#include <vector>

namespace sky::aurora {

    class AuroraImageBuilder : public AssetBuilder {
    public:
        AuroraImageBuilder()           = default;
        ~AuroraImageBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &) const override;

        void LoadConfig(const FileSystemPtr &cfg) override;
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

    private:
        std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".hdr", ".ktx", ".image"};
    };

} // namespace sky::aurora
