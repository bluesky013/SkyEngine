//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Sampler.h>
#include <GLESForward.h>

namespace sky::aurora {

    class GLESDevice;

    class GLESSampler : public Sampler {
    public:
        explicit GLESSampler(GLESDevice &dev);
        ~GLESSampler() override;

        bool Init(const Descriptor &desc);

        GLuint GetNativeHandle() const { return sampler; }

    private:
        GLESDevice &device;
        GLuint sampler = 0;
    };

} // namespace sky::aurora
