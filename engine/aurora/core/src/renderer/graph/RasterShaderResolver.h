//
// Created on 2026/10/16.
//

#pragma once

#include <aurora/rdg/FrameExecutionTypes.h>
#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderResolver.h>

namespace sky::aurora {
    struct RasterDynamicUniform {
        uint32_t set = 0, binding = 0;
        bool     operator==(const RasterDynamicUniform &) const = default;
    };
    struct RasterShaderRequest {
        ShaderResolver::Request           vertex, fragment;
        std::vector<RasterDynamicUniform> dynamicUniforms;
    };
    // Resolve/build on the coordinator before graph freeze; encode workers only retain the resulting shader.
    std::variant<FrameError, ShaderPtr> ResolveRasterShader(Device &, ShaderResolver &, const RasterShaderRequest &);
} // namespace sky::aurora
