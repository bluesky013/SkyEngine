//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class Sampler {
    public:
        Sampler() = default;
        virtual ~Sampler() = default;

        struct Descriptor {
            Filter    magFilter        = Filter::LINEAR;
            Filter    minFilter        = Filter::LINEAR;
            MipFilter mipmapMode       = MipFilter::NEAREST;
            WrapMode  addressModeU     = WrapMode::REPEAT;
            WrapMode  addressModeV     = WrapMode::REPEAT;
            WrapMode  addressModeW     = WrapMode::REPEAT;
            float     minLod           = 0.f;
            float     maxLod           = 0.25f;
            float     maxAnisotropy    = 1.f;
            bool      anisotropyEnable = false;
        };
    };
    using SamplerPtr = std::shared_ptr<Sampler>;
}
