//
// Created on 2026/04/02.
//

#include <MetalSampler.h>

#include "MetalUtils.h"
#include <MetalDevice.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalSampler::MetalSampler(MetalDevice &dev) : device(dev)
    {
    }

    const Device *MetalSampler::GetDevice() const noexcept
    {
        return &device;
    }

    MetalSampler::~MetalSampler()
    {
        if (sampler != nullptr) {
            (void)(__bridge_transfer id<MTLSamplerState>)sampler;
            sampler = nullptr;
        }
    }

    bool MetalSampler::Init(const Descriptor &desc)
    {
        auto *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for sampler creation");
            return false;
        }
        const bool extendedModes = SupportsMetalFamily7OrMac2(metalDevice);
        const auto addressU      = ToMetalAddressMode(desc.addressModeU, extendedModes);
        const auto addressV      = ToMetalAddressMode(desc.addressModeV, extendedModes);
        const auto addressW      = ToMetalAddressMode(desc.addressModeW, extendedModes);
        if (!addressU || !addressV || !addressW) {
            LOG_E(TAG, "unsupported Metal sampler address mode");
            return false;
        }

        const bool anisotropyEnabled = desc.anisotropyEnable && device.GetCapability().anisotropyEnable;
        if (desc.anisotropyEnable && !anisotropyEnabled) {
            LOG_W(TAG, "sampler anisotropy requested but not supported by the current Metal device; falling back to anisotropy disabled");
        }

        auto *samplerDesc         = [[MTLSamplerDescriptor alloc] init];
        samplerDesc.minFilter     = ToMetalFilter(desc.minFilter);
        samplerDesc.magFilter     = ToMetalFilter(desc.magFilter);
        samplerDesc.mipFilter     = ToMetalMipFilter(desc.mipmapMode);
        samplerDesc.sAddressMode  = *addressU;
        samplerDesc.tAddressMode  = *addressV;
        samplerDesc.rAddressMode  = *addressW;
        samplerDesc.lodMinClamp   = desc.minLod;
        samplerDesc.lodMaxClamp   = desc.maxLod;
        samplerDesc.maxAnisotropy = anisotropyEnabled ? static_cast<NSUInteger>(desc.maxAnisotropy) : 1U;

        auto *nativeSampler = [metalDevice newSamplerStateWithDescriptor:samplerDesc];

        if (nativeSampler == nil) {
            LOG_E(TAG, "newSamplerStateWithDescriptor failed");
            return false;
        }

        sampler = (__bridge_retained void *)nativeSampler;
        return true;
    }

} // namespace sky::aurora
