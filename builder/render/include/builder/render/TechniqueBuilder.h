//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <string_view>
#include <framework/asset/AssetBuilder.h>

namespace sky::builder {

    class TechniqueBuilder : public AssetBuilder {
    public:
        TechniqueBuilder() = default;
        ~TechniqueBuilder() = default;

        static constexpr char* KEY = "GFX_TECH";

        void Request(const BuildRequest &build, BuildResult &result) override;
    };

}
