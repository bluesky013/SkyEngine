//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

namespace sky::builder {

    class ShaderBuilder : public AssetBuilder {
    public:
        ShaderBuilder() = default;
        ~ShaderBuilder() = default;

        void Request(BuildRequest &build) override;
    };

}