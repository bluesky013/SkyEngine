//
// Created by Zach on 2023/2/6.
//

#include <gles/Sampler.h>
#include <gles/Conversion.h>

namespace sky::gles {
    Sampler::~Sampler()
    {
        if (sampler != 0) {
            glDeleteSamplers(1, &sampler);
        }
    }

    bool Sampler::Init(const Descriptor &desc)
    {
        parameter.minFilter = FromRHI(desc.minFilter, desc.mipmapMode);
        parameter.magFilter = FromRHI(desc.magFilter);
        parameter.wrapS = FromRHI(desc.addressModeU);
        parameter.wrapT = FromRHI(desc.addressModeV);
        parameter.wrapR = FromRHI(desc.addressModeW);
        parameter.minLod = desc.minLod;
        parameter.maxLod = desc.maxLod;

        CHECK(glGenSamplers(1, &sampler));
        CHECK(glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, parameter.minFilter));
        CHECK(glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, parameter.magFilter));
        CHECK(glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, parameter.wrapS));
        CHECK(glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, parameter.wrapT));
        CHECK(glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, parameter.wrapR));
        CHECK(glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, parameter.minLod));
        CHECK(glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, parameter.maxLod));

        return true;
    }

}
