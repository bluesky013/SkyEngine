//
// Created on 2026/04/01.
//

#include <GLESShader.h>
#include <GLESLoader.h>
#include <GLESDevice.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraGL";

namespace sky::aurora {

    // -----------------------------------------------------------------------
    // GLESShaderFunction
    // -----------------------------------------------------------------------
    GLESShaderFunction::GLESShaderFunction(GLESDevice &dev)
        : device(dev)
    {
    }

    GLESShaderFunction::~GLESShaderFunction()
    {
        if (shader != 0) {
            glDeleteShader(shader);
        }
    }

    bool GLESShaderFunction::Init(const Descriptor &desc)
    {
        GLenum type = GL_VERTEX_SHADER;
        switch (desc.stage) {
        case ShaderStageFlagBit::FS: type = GL_FRAGMENT_SHADER; break;
        case ShaderStageFlagBit::CS: type = GL_COMPUTE_SHADER;  break;
        default: break;
        }

        shader = glCreateShader(type);
        if (shader == 0) {
            LOG_E(TAG, "glCreateShader failed");
            return false;
        }

        auto *binaryProvider = dynamic_cast<ShaderBinaryProvider *>(desc.data.Get());
        if (binaryProvider == nullptr || binaryProvider->binaryData == nullptr) {
            LOG_E(TAG, "shader function missing binary data");
            glDeleteShader(shader);
            shader = 0;
            return false;
        }

        const auto &binData = binaryProvider->binaryData;
        const char *source = reinterpret_cast<const char *>(binData->Data());
        const GLint length = static_cast<GLint>(binData->Size());

        glShaderSource(shader, 1, &source, &length);
        glCompileShader(shader);

        GLint status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            GLint logLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
            if (logLen > 0) {
                std::unique_ptr<char[]> log(new char[logLen + 1]);
                glGetShaderInfoLog(shader, logLen, nullptr, log.get());
                LOG_E(TAG, "shader compile failed:\n%s", log.get());
            }
            glDeleteShader(shader);
            shader = 0;
            return false;
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // GLESShader
    // -----------------------------------------------------------------------
    GLESShader::GLESShader(GLESDevice &dev)
        : device(dev)
    {
    }

    GLESShader::~GLESShader()
    {
        if (program != 0) {
            glDeleteProgram(program);
        }
    }

    bool GLESShader::Init(const Descriptor &desc)
    {
        program = glCreateProgram();
        if (program == 0) {
            LOG_E(TAG, "glCreateProgram failed");
            return false;
        }

        if (desc.vs != nullptr) {
            auto *glesVS = static_cast<GLESShaderFunction *>(desc.vs);
            glAttachShader(program, glesVS->GetNativeHandle());
        }
        if (desc.ps != nullptr) {
            auto *glesPS = static_cast<GLESShaderFunction *>(desc.ps);
            glAttachShader(program, glesPS->GetNativeHandle());
        }

        glLinkProgram(program);

        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            GLint logLen = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
            if (logLen > 0) {
                std::unique_ptr<char[]> log(new char[logLen + 1]);
                glGetProgramInfoLog(program, logLen, nullptr, log.get());
                LOG_E(TAG, "program link failed:\n%s", log.get());
            }
            glDeleteProgram(program);
            program = 0;
            return false;
        }

        // Detach shaders after linking
        if (desc.vs != nullptr) {
            glDetachShader(program, static_cast<GLESShaderFunction *>(desc.vs)->GetNativeHandle());
        }
        if (desc.ps != nullptr) {
            glDetachShader(program, static_cast<GLESShaderFunction *>(desc.ps)->GetNativeHandle());
        }

        return true;
    }

} // namespace sky::aurora
