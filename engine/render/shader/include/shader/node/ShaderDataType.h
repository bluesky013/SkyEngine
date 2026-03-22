//
// Created by blues on 2025/3/28.
//

#pragma once

#include <cstdint>
#include <span>
#include <string_view>

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

    enum class LayoutStandard : uint8_t {
        STD140,
        STD430,
    };

    enum class StructUsage : uint8_t {
        PLAIN,   // Nested / member struct -- layout determined by parent context
        UBO,     // Maps to cbuffer (std140)
        SSBO,    // Maps to StructuredBuffer (std430)
    };

    struct ValueType {
        ShaderBaseType baseType  = ShaderBaseType::FLOAT;
        ShaderDataType dataType  = ShaderDataType::SCALAR;
        uint8_t        row       = 1;
        uint8_t        column    = 1;
    };

    struct MemberDecl {
        std::string_view  name;
        ValueType         type;
        uint32_t          arraySize = 0;
        std::string_view  structRef;
    };

    struct StructDecl {
        std::string_view              name;
        std::span<const MemberDecl>   members;
        StructUsage                   usage = StructUsage::PLAIN;
    };

    constexpr LayoutStandard GetLayoutStandard(StructUsage usage)
    {
        return (usage == StructUsage::SSBO) ? LayoutStandard::STD430 : LayoutStandard::STD140;
    }

    constexpr LayoutStandard GetLayoutStandard(const StructDecl &decl)
    {
        return GetLayoutStandard(decl.usage);
    }

    struct LayoutInfo {
        uint32_t size      = 0;
        uint32_t alignment = 0;
        uint32_t stride    = 0;
        uint32_t offset    = 0;
    };

} // namespace sky::sl
