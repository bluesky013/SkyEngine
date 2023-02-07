//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/GraphicsPipeline.h>
#include <gles/Shader.h>
#include <gles/Core.h>
#include <core/logger/Logger.h>
#include <unordered_set>

static const char* TAG = "GLES";

namespace sky::gles {

    static std::unordered_set<GLenum> SAMPLER_TYPE = {
        GL_SAMPLER_2D,
        GL_SAMPLER_3D,
        GL_SAMPLER_CUBE,
        GL_SAMPLER_2D_SHADOW,
        GL_SAMPLER_2D_ARRAY,
        GL_SAMPLER_2D_ARRAY_SHADOW,
        GL_SAMPLER_CUBE_SHADOW,
        GL_INT_SAMPLER_2D,
        GL_INT_SAMPLER_3D,
        GL_INT_SAMPLER_CUBE,
        GL_INT_SAMPLER_2D_ARRAY,
        GL_UNSIGNED_INT_SAMPLER_2D,
        GL_UNSIGNED_INT_SAMPLER_3D,
        GL_UNSIGNED_INT_SAMPLER_CUBE,
        GL_UNSIGNED_INT_SAMPLER_2D_ARRAY
    };

    bool GraphicsPipeline::InitProgram(const Descriptor &desc)
    {
        program = glCreateProgram();

        auto attachShader = [this](const rhi::ShaderPtr &shader)
        {
            ShaderPtr esShader = std::static_pointer_cast<Shader>(shader);
            CHECK(glAttachShader(program, esShader->GetNativeHandle()));
        };
        attachShader(desc.vs);
        attachShader(desc.fs);

        CHECK(glLinkProgram(program));

        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            GLint logSize = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
            std::unique_ptr<GLchar[]> data = std::make_unique<GLchar[]>(logSize + 1);
            CHECK(glGetProgramInfoLog(program, logSize, nullptr, data.get()));

            LOG_E(TAG, "link program failed. error%s", data.get());
            return false;
        }

        GLint value = 0;

        glGetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &value);
        LOG_I(TAG, "active uniform block %d", value);
        for (GLint i = 0; i < value; ++i) {
            GLsizei length = 0;
            char test[32];
            glGetActiveUniformBlockName(program, i, 255, &length, test);
            GLuint index = glGetUniformBlockIndex(program, test);
            glUniformBlockBinding(program, index, 0);

            LOG_I(TAG, "uniform block index %d, length %d, %s, index%u", i, length, test, index);
        }

        glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &value);
        glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &value);
        for (GLint i = 0; i < value; ++i) {
            GLsizei length = 0;
            char test[32];
            GLsizei size = 0;
            GLenum type = 0;
            glGetActiveUniform(program, i, 255, &length, &size, &type, test);
            LOG_I(TAG, "uniform index %d, length %d, %s", i, length, test);
        }


        return true;
    }

    bool GraphicsPipeline::Init(const Descriptor &desc)
    {
        if (!InitProgram(desc)) {
            return false;
        }
        state = desc.state;
        return true;
    }

}