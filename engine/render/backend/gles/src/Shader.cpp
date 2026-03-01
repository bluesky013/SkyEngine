//
// Created by Zach Lee on 2023/2/2.
//

#include <gles/Shader.h>
#include <gles/Core.h>
#include <core/logger/Logger.h>
#include <string>

static const char* TAG = "GLES";

namespace sky::gles {

    Shader::~Shader()
    {
        if (shader != 0) {
            glDeleteShader(shader);
        }
    }

    bool Shader::Init(const Descriptor &desc)
    {
        GLenum shaderType = GL_VERTEX_SHADER;
        if (desc.stage == rhi::ShaderStageFlagBit::FS) shaderType = GL_FRAGMENT_SHADER;
        else if (desc.stage == rhi::ShaderStageFlagBit::CS) shaderType = GL_COMPUTE_SHADER;

        shader = glCreateShader(shaderType);
        const char *source = reinterpret_cast<const char*>(desc.data);
        CHECK(glShaderSource(shader, 1, (const GLchar **)&source, nullptr));
        CHECK(glCompileShader(shader));

        GLint status = 0;
        CHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));
        if (status != GL_TRUE) {
            GLint logSize = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
            std::unique_ptr<GLchar[]> data = std::make_unique<GLchar[]>(logSize + 1);
            CHECK(glGetShaderInfoLog(shader, logSize, nullptr, data.get()));

            LOG_E(TAG, "compile shader failed. source\n%s, error%s", source, data.get());
            return false;
        }

        return true;
    }

}