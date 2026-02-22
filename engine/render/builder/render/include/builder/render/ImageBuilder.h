//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <render/adaptor/assets/ImageAsset.h>
#include <builder/render/image/ImageProcess.h>
#include <rhi/Core.h>

#include <string_view>
#include <unordered_map>

namespace sky::builder {
    class ImageBuilder : public AssetBuilder {
    public:
        ImageBuilder();
        ~ImageBuilder() override = default;

        void UseCompress(bool en) { compress = en; }
    private:
        void RequestDDS(const AssetBuildRequest &request, AssetBuildResult &result);
        void RequestSTB(const AssetBuildRequest &request, AssetBuildResult &result);
        void RequestImage(const AssetBuildRequest &request, AssetBuildResult &result);

        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override;
        void LoadConfig(const FileSystemPtr &cfg) override;

        const std::vector<std::string> &GetExtensions() const override { return extensions; }
        std::string_view QueryType(const std::string &ext) const override { return AssetTraits<Texture>::ASSET_TYPE; }

        std::vector<std::string> extensions = {".jpg", ".dds", ".ktx", ".png", ".hdr", ".image"};
        std::unordered_map<std::string, ImageBuildGlobalConfig> configs;

        bool compress = true;
    };
}
