//
// Created by Zach Lee on 2022/1/31.
//

#pragma once

#include <string>
#include <vector>
#include <engine/asset/ShaderAsset.h>
#include <framework/interface/IBuilder.h>

namespace sky {

    class ShaderBuilder : public IBuilder {
    public:
        ShaderBuilder();
        ~ShaderBuilder();

        bool Build(const BuildRequest& request) override;

        bool Support(const std::string& ext) const override;

    };
}