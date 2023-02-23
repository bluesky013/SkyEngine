//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

namespace sky::builder {

    class TechniqueBuilder : public AssetBuilder {
    public:
        TechniqueBuilder() = default;
        ~TechniqueBuilder() = default;

        void Request(BuildRequest &build) override;
    };

}