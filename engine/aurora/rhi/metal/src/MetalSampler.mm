//
// Created on 2026/04/02.
//

#include <MetalSampler.h>
#include <MetalDevice.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalSampler::MetalSampler(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalSampler::~MetalSampler()
    {
        if (sampler != nullptr) {
            [(id<MTLSamplerState>)sampler release];
            sampler = nullptr;
        }
    }

    bool MetalSampler::Init(const Descriptor &desc)
    {
        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for sampler creation");
            return false;
        }

        const bool anisotropyEnabled = desc.anisotropyEnable && device.GetCapability().anisotropyEnable;
        if (desc.anisotropyEnable && !anisotropyEnabled) {
            LOG_W(TAG, "sampler anisotropy requested but not supported by the current Metal device; falling back to anisotropy disabled");
        }

        auto *samplerDesc = [[MTLSamplerDescriptor alloc] init];
        samplerDesc.minFilter = ToMetalFilter(desc.minFilter);
        samplerDesc.magFilter = ToMetalFilter(desc.magFilter);
        samplerDesc.mipFilter = ToMetalMipFilter(desc.mipmapMode);
        samplerDesc.sAddressMode = ToMetalAddressMode(desc.addressModeU);
        samplerDesc.tAddressMode = ToMetalAddressMode(desc.addressModeV);
        samplerDesc.rAddressMode = ToMetalAddressMode(desc.addressModeW);
        samplerDesc.lodMinClamp = desc.minLod;
        samplerDesc.lodMaxClamp = desc.maxLod;
        samplerDesc.maxAnisotropy = anisotropyEnabled ? static_cast<NSUInteger>(desc.maxAnisotropy) : 1U;

        auto *nativeSampler = [metalDevice newSamplerStateWithDescriptor:samplerDesc];
        [samplerDesc release];

        if (nativeSampler == nil) {
            LOG_E(TAG, "newSamplerStateWithDescriptor failed");
            return false;
        }

        sampler = nativeSampler;
        return true;
    }

} // namespace sky::aurora