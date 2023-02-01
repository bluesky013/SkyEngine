//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/GraphicsPipeline.h>
#include <gles/Shader.h>
#include <gles/Core.h>
#include <core/logger/Logger.h>

static const char* TAG = "GLES";

namespace sky::gles {

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