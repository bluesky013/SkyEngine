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
        AuroraImageBuilder();
        ~AuroraImageBuilder() override = default;

        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &) const override;

        void LoadConfig(const FileSystemPtr &cfg) override;
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;

        // Effective image settings for a product bundle (from image_build_presets.json).
        std::vector<std::pair<std::string, std::string>> DescribeSettings(const ProductBundleKey &bundle) const override;
        // Effective settings with a per-asset sparse override applied.
        std::vector<std::pair<std::string, std::string>> DescribeSettings(const ProductBundleKey      &bundle,
                                                                          const BuildSettingsOverride &override) const override;
        // Reflected editor-facing settings type / materialization / diff.
        const TypeInfoRT     *GetSettingsType() const override;
        Any                   MakeSettings(const ProductBundleKey &bundle, const BuildSettingsOverride &override) const override;
        BuildSettingsOverride DiffSettings(const ProductBundleKey &bundle, const Any &edited) const override;

    private:
        std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".hdr", ".ktx", ".image"};
        cook::ImageBuildPresets  presets;
    };

} // namespace sky::aurora
