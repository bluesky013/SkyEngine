//
// Created by Zach Lee on 2023/2/26.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

namespace sky::builder {
    class ImageBuilder : public AssetBuilder {
    public:
        ImageBuilder() = default;
        ~ImageBuilder() = default;

        static constexpr char* KEY = "GFX_IMAGE";

        void Request(const BuildRequest &build, BuildResult &result) override;
    };
}