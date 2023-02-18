//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <vector>

namespace sky::rhi {

    class Shader {
    public:
        Shader() = default;
        virtual ~Shader() = default;

        struct Descriptor {
            ShaderStageFlagBit stage;
            std::vector<uint8_t> data;
        };
    };
    using ShaderPtr = std::shared_ptr<Shader>;
}