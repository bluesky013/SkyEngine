//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Resource.h>

namespace sky::aurora {

    class Sampler
        : public RefObject
        , public IDelayReleaseResource {
    public:
        struct Descriptor {
        };

        Sampler() = default;
        ~Sampler() override = default;
    };

    using SamplerPtr = CounterPtr<Sampler>;

} // namespace sky::aurora
