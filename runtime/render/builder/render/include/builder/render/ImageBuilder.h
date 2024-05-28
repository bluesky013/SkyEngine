//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <string_view>
#include <rhi/Core.h>
#include <unordered_map>

namespace sky::builder {

    struct ImageBuildConfig {
        PlatformType platform = PlatformType::Default;
        bool compress = true;
        bool generateMip = true;
        uint32_t maxWidth = 0xFFFFFFFF;
        uint32_t maxHeight = 0xFFFFFFFF;
        rhi::PixelFormat alphaFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;
        rhi::PixelFormat opaqueFormat = rhi::PixelFormat::BC7_UNORM_BLOCK;
        rhi::PixelFormat hdrFormat = rhi::PixelFormat::BC6H_SFLOAT_BLOCK;
    };

    class ImageBuilder : public AssetBuilder {
    public:
        ImageBuilder();
        ~ImageBuilder() override = default;

        static constexpr std::string_view KEY = "GFX_IMAGE";

        void Request(const BuildRequest &build, BuildResult &result) override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }

        void UseCompress(bool en) { compress = en; }
    private:
        void RequestDDS(const BuildRequest &build, BuildResult &result);
        void RequestSTB(const BuildRequest &build, BuildResult &result);

        std::string GetConfigKey() const override { return "Texture"; }
        void LoadConfig(const FileSystemPtr &fs, const std::string &path) override;

        std::vector<std::string> extensions = {".jpg", ".dds", ".ktx", ".png"};
        std::unordered_map<std::string, ImageBuildConfig> configs;

        bool compress = false;
    };
}
