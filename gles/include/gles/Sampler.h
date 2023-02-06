//
// Created by Zach on 2023/2/6.
//

#pragma once
#include <rhi/Sampler.h>
#include <gles/DevObject.h>

namespace sky::gles {

    struct SamplerParameter {
        GLenum minFilter = 0;
        GLenum magFilter = 0;
        GLenum wrapS = 0;
        GLenum wrapT = 0;
        GLenum wrapR = 0;
        float minLod = 0.f;
        float maxLod = 0.25f;
    };

    class Sampler : public rhi::Sampler, public DevObject {
    public:
        Sampler(Device &dev) : DevObject(dev) {}
        ~Sampler();

    private:
        bool Init(const Descriptor &);

        GLuint sampler = 0;
        SamplerParameter parameter;
    };
    using SamplerPtr = std::shared_ptr<Sampler>;
}
