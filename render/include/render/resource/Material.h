//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <unordered_map>
#include <core/type/Any.h>
#include <render/resource/Texture.h>

namespace sky {

    class Material {
    public:
        Material()  = default;
        ~Material() = default;

    private:
        std::unordered_map<std::string, Any> properties;
    };
} // namespace sky