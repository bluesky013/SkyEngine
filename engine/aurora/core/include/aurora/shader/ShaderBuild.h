//
// ShaderBuild: turn a compiled (blob + reflection) result into RHI objects.
// Device creation must happen on the render thread.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderCompile.h>

#include <string>

namespace sky::aurora {

    class Device;

    // Build a ShaderFunction (stage + entry) from a per-stage compile result.
    ShaderFunctionPtr
    CreateShaderFunctionFromResult(Device &device, const ShaderCompileResult &result, ShaderStageFlagBit stage, const std::string &entry);

} // namespace sky::aurora
