//
// Created by Zach Lee on 2023/2/2.
//

#pragma once

#include <rhi/Shader.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class Shader : public rhi::Shader, public DevObject {
    public:
        Shader(Device &dev) : DevObject(dev) {}
        ~Shader();

        bool Init(const Descriptor &desc);
        GLuint GetNativeHandle() const { return shader; }

    private:
        GLuint shader = 0;
    };
    using ShaderPtr = std::shared_ptr<Shader>;

}