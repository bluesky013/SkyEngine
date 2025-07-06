//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/Sampler.h>
#include <mtl/Device.h>
#include <mtl/Conversion.h>

namespace sky::mtl {

    Sampler::~Sampler()
    {
        if (state != nil) {
            [state release];
            state = nil;
        }
    }

    bool Sampler::Init(const Descriptor &desc)
    {
        auto *samplerDesc = [[MTLSamplerDescriptor alloc] init];

        samplerDesc.minFilter = FromRHI(desc.minFilter);
        samplerDesc.magFilter = FromRHI(desc.magFilter);
        samplerDesc.mipFilter = FromRHI(desc.mipmapMode);
        samplerDesc.lodMinClamp = desc.minLod;
        samplerDesc.lodMaxClamp = desc.maxLod;
        samplerDesc.maxAnisotropy = desc.anisotropyEnable ? desc.maxAnisotropy : 1.f;
        samplerDesc.rAddressMode = FromRHI(desc.addressModeU);
        samplerDesc.sAddressMode = FromRHI(desc.addressModeV);
        samplerDesc.tAddressMode = FromRHI(desc.addressModeW);


        state = [device.GetMetalDevice() newSamplerStateWithDescriptor: samplerDesc];
        [samplerDesc release];
        return state != nil;
    }

} // namespace sky::mtl
