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
    };
}
