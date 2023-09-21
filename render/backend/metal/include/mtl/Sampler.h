//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/Sampler.h>
#include <mtl/DevObject.h>
#import <Metal/MTLSampler.h>

namespace sky::mtl {
    class Device;

    class Sampler : public rhi::Sampler, public DevObject {
    public:
        Sampler(Device &dev) : DevObject(dev) {}
        ~Sampler();

    private:
        friend class Device;
        bool Init(const Descriptor &);

        id<MTLSamplerState> state = nil;
    };

} // namespace sky::mtl
