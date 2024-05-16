//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <string_view>

namespace sky::builder {
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

        std::vector<std::string> extensions = {".jpg", ".dds", ".ktx", ".png"};

        bool compress = false;
    };
}
