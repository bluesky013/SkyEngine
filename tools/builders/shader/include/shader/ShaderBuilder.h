//
// Created by Zach Lee on 2022/1/31.
//

#pragma once

#include <string>
#include <vector>
#include <engine/asset/ShaderAsset.h>
#include <BuilderBase.h>

namespace sky {

    class ShaderBuilder : public BuilderBase {
    public:
        ShaderBuilder();
        ~ShaderBuilder();

        bool Build(const BuildRequest& request) override;

    private:
        ShaderSourceData sourceData;
    };
}