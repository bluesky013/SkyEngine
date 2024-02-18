//
// Created by blues on 2024/2/18.
//

#pragma once

#include <shader/ShaderCompiler.h>

namespace sky {

    void BuildReflectionSPIRV(rhi::ShaderStageFlagBit stage, ShaderBuildResult &result);

} // namespace sky