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

        bool Load(const std::string& path);

        void Save(const std::string& path);

    private:
        ShaderAssetPtr asset;
    };

}