//
// Created by Zach Lee on 2022/1/16.
//

#pragma once
#include <string>
#include <engine/asset/ShaderAsset.h>

namespace sky {

    class ShaderLoader {
    public:
        ShaderLoader() = default;
        ~ShaderLoader() = default;

        void Load(const std::string& path, ShaderAsset::SourceData& data);
    };

}