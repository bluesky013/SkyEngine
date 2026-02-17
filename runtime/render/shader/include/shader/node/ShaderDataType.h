//
// Created by blues on 2025/3/28.
//

#pragma once

namespace sky::sl {

    enum class ShaderBaseType : uint8_t {
        NONE,
        BOOL,
        UINT,
        INT,
        HALF,
        FLOAT,
        DOUBLE,
    };

    enum class ShaderDataType : uint8_t {
        VOID,
        SCALAR,
        VECTOR,
        MATRIX,
        STRUCT,
        BUFFER,
        TEXTURE,
        SAMPLER
    };

    enum class TextureType : uint8_t {
        NONE,
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_3D,
        TEXTURE_CUBE,
        TEXTURE_2D_ARRAY,
        TEXTURE_CUBE_ARRAY,
    };

} // namespace sky::sl
