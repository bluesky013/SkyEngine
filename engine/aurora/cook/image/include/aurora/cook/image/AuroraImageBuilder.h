//
// Aurora image asset builder: decode -> resize -> linearize -> mip -> compress
// -> aurora ImageAssetData, written to the bundle resolved from
// configs/image_build_presets.json.
//

#pragma once

#include <aurora/cook/image/ImageBuildConfig.h>
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
        cook::ImageBuildPresets  presets;
    };

} // namespace sky::aurora
