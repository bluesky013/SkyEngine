//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Resource.h>

namespace sky::aurora {

    class Sampler
        : public RefObject
        , public IDelayReleaseResource {
    public:
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

        Sampler() = default;
        ~Sampler() override = default;
    };

    using SamplerPtr = CounterPtr<Sampler>;

} // namespace sky::aurora
