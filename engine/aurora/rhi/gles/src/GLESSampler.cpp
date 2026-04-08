//
// Created on 2026/04/01.
//

#include <GLESSampler.h>
#include <GLESLoader.h>
#include <GLESDevice.h>
#include <GLESConversion.h>
#include <core/logger/Logger.h>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

static const char *TAG = "AuroraGL";

namespace sky::aurora {

    GLESSampler::GLESSampler(GLESDevice &dev)
        : device(dev)
    {
    }

    GLESSampler::~GLESSampler()
    {
        if (sampler != 0) {
            glDeleteSamplers(1, &sampler);
        }
    }

    bool GLESSampler::Init(const Descriptor &desc)
    {
        const bool anisotropyEnabled = desc.anisotropyEnable && device.GetCapability().anisotropyEnable;

        glGenSamplers(1, &sampler);
        if (sampler == 0) {
            LOG_E(TAG, "glGenSamplers failed");
            return false;
        }

        glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER,
            FromFilterMip(desc.minFilter, desc.mipmapMode));
        glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER,
            FromFilter(desc.magFilter));
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S,
            FromWrapMode(desc.addressModeU));
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T,
            FromWrapMode(desc.addressModeV));
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R,
            FromWrapMode(desc.addressModeW));
        glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, desc.minLod);
        glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, desc.maxLod);

        if (anisotropyEnabled) {
            glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, desc.maxAnisotropy);
        } else if (desc.anisotropyEnable) {
            LOG_W(TAG, "sampler anisotropy requested but not supported by the current GLES device; falling back to anisotropy disabled");
        }

        return true;
    }

} // namespace sky::aurora
