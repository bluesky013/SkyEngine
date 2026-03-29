//
// Created by blues on 2026/3/29.
//

#pragma once

#include <core/template/Flags.h>

namespace sky {

    enum class ShaderLanguage : uint32_t {
        HLSL,
        GLSL
    };

    enum class ShaderCompileTarget : uint32_t {
        SPIRV,
        MSL,
        DXIL,
        NUM
    };

    enum class BaseType : uint32_t {
        UNDEFINED = 0,
        FLOAT,
        INT,
        UINT,
    };


    enum class ShaderResourceType : uint32_t {
        SAMPLER                = 0,
        SAMPLED_IMAGE          = 1,
        STORAGE_IMAGE          = 2,
        UNIFORM_BUFFER         = 3,
        STORAGE_BUFFER         = 4,
        INPUT_ATTACHMENT       = 5,
    };

    enum class ShaderStageFlagBit : uint32_t {
        VS  = 0x01,
        FS  = 0x02,
        CS  = 0x04,
        TAS = 0x80,
        MS  = 0x100,
        GFX = VS | FS
    };
    using ShaderStageFlags = Flags<ShaderStageFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(ShaderStageFlagBit)

} // namespace sky