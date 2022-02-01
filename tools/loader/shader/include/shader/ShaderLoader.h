//
// Created by Zach Lee on 2022/1/31.
//

#pragma once

#include <string>
#include <vector>
#include <engine/asset/ShaderAsset.h>

namespace sky {

    class ShaderLoader {
    public:
        ShaderLoader();
        ~ShaderLoader();

        bool Load(const std::string& path);

        void Save(const std::string& path);

    private:
        ShaderAssetPtr asset;
    };

}