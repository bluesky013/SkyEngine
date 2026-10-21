#pragma once

#include <aurora/rhi/ShaderReflection.h>
#include <aurora/shader/RgBlockDesc.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <cstddef>
#include <type_traits>

namespace sky::aurora::shared {
    template <RgFieldType>
    struct FieldTraits;

#define AURORA_FIELD_TRAITS(tag, cpp, alignment, scalar, shape, rowCount, columnCount)                                                               \
    template <>                                                                                                                                      \
    struct FieldTraits<RgFieldType::tag> {                                                                                                           \
        using Type                                   = cpp;                                                                                          \
        static constexpr size_t           align      = alignment;                                                                                    \
        static constexpr ShaderScalarType scalarType = ShaderScalarType::scalar;                                                                     \
        static constexpr ShaderTypeKind   kind       = ShaderTypeKind::shape;                                                                        \
        static constexpr uint32_t         rows = rowCount, cols = columnCount;                                                                       \
    };
    AURORA_FIELD_TRAITS(FLOAT, float, 4, FLOAT, SCALAR, 1, 1)
    AURORA_FIELD_TRAITS(FLOAT2, Vector2, 8, FLOAT, VECTOR, 1, 2)
    AURORA_FIELD_TRAITS(FLOAT3, Vector3, 16, FLOAT, VECTOR, 1, 3)
    AURORA_FIELD_TRAITS(FLOAT4, Vector4, 16, FLOAT, VECTOR, 1, 4)
    AURORA_FIELD_TRAITS(MAT4, Matrix4, 16, FLOAT, MATRIX, 4, 4)
    AURORA_FIELD_TRAITS(INT, int32_t, 4, INT, SCALAR, 1, 1)
    AURORA_FIELD_TRAITS(UINT, uint32_t, 4, UINT, SCALAR, 1, 1)
#undef AURORA_FIELD_TRAITS

    template <RgFieldType Type, class Member>
    ShaderBlockMember DescribeMember(const char *name, uint32_t offset)
    {
        using Traits         = FieldTraits<Type>;
        constexpr auto count = std::extent_v<Member>;
        static_assert(Type != RgFieldType::FLOAT3, "Use FLOAT4 for portable uniforms; Metal float3 has a different size");
        static_assert(count == 0 || (Type == RgFieldType::FLOAT4 && count <= 4096), "Only bounded float4 uniform arrays are supported");
        return {.name        = name,
                .offset      = offset,
                .size        = sizeof(Member),
                .scalarType  = Traits::scalarType,
                .kind        = count ? ShaderTypeKind::ARRAY : Traits::kind,
                .rows        = Traits::rows,
                .cols        = Traits::cols,
                .arrayCount  = count,
                .arrayStride = count ? sizeof(typename Traits::Type) : 0};
    }

    template <RgFieldType Type>
    constexpr ShaderTextureType DescribeTexture(ShaderTextureDimension dimension, bool arrayed = false)
    {
        using Traits = FieldTraits<Type>;
        static_assert(Traits::kind == ShaderTypeKind::SCALAR || Traits::kind == ShaderTypeKind::VECTOR);
        return {dimension, Traits::scalarType, Traits::cols, arrayed, false};
    }
} // namespace sky::aurora::shared
