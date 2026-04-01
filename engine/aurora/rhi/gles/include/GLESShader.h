//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Shader.h>
#include <GLESForward.h>

namespace sky::aurora {

    class GLESDevice;

    class GLESShaderFunction : public ShaderFunction {
    public:
        explicit GLESShaderFunction(GLESDevice &dev);
        ~GLESShaderFunction() override;

        bool Init(const Descriptor &desc);

        GLuint GetNativeHandle() const { return shader; }

    private:
        GLESDevice &device;
        GLuint shader = 0;
    };

    class GLESShader : public Shader {
    public:
        explicit GLESShader(GLESDevice &dev);
        ~GLESShader() override;

        bool Init(const Descriptor &desc);

        GLuint GetProgram() const { return program; }

    private:
        GLESDevice &device;
        GLuint program = 0;
    };

} // namespace sky::aurora
