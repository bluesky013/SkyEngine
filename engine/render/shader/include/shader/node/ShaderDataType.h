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

    // -- constexpr layout calculation ---------------------------------

    constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    constexpr uint32_t BaseTypeSize(ShaderBaseType baseType)
    {
        switch (baseType) {
        case ShaderBaseType::BOOL:
        case ShaderBaseType::UINT:
        case ShaderBaseType::INT:
        case ShaderBaseType::FLOAT:
            return 4;
        case ShaderBaseType::HALF:
            return 2;
        case ShaderBaseType::DOUBLE:
            return 8;
        default:
            return 0;
        }
    }

    constexpr LayoutInfo CalculateTypeLayout(const ValueType &type, LayoutStandard std)
    {
        switch (type.dataType) {
        case ShaderDataType::SCALAR: {
            uint32_t sz = BaseTypeSize(type.baseType);
            return {sz, sz, 0, 0};
        }
        case ShaderDataType::VECTOR: {
            uint32_t base = BaseTypeSize(type.baseType);
            uint32_t n    = type.row;
            uint32_t align = (n <= 2) ? base * 2 : base * 4;
            return {base * n, align, 0, 0};
        }
        case ShaderDataType::MATRIX: {
            uint32_t base    = BaseTypeSize(type.baseType);
            uint32_t rows    = type.row;
            uint32_t cols    = type.column;
            uint32_t colAlign = (rows <= 2) ? base * 2 : base * 4;
            uint32_t colSize  = base * rows;
            uint32_t padCol   = AlignUp(colSize, colAlign);
            return {padCol * cols, colAlign, 0, 0};
        }
        default:
            return {};
        }
    }

    // Forward-declare: find struct in a flat span (constexpr-friendly)
    constexpr const StructDecl *FindStruct(std::string_view name,
                                           std::span<const StructDecl> structs)
    {
        for (const auto &s : structs) {
            if (s.name == name) return &s;
        }
        return nullptr;
    }

    // Forward declarations for mutual recursion
    constexpr LayoutInfo CalculateStructLayout(const StructDecl &decl,
                                               std::span<const StructDecl> structs);
    constexpr uint32_t   CalculateMembersSize(std::span<const MemberDecl> members,
                                               std::span<const StructDecl> structs,
                                               LayoutStandard std);

    // Element layout for one member (handles struct refs, NOT arrays)
    constexpr LayoutInfo ElementLayout(const MemberDecl &m,
                                       std::span<const StructDecl> structs,
                                       LayoutStandard std)
    {
        if (m.type.dataType == ShaderDataType::STRUCT && !m.structRef.empty()) {
            const auto *sd = FindStruct(m.structRef, structs);
            if (sd) return CalculateStructLayout(*sd, structs);
        }
        return CalculateTypeLayout(m.type, std);
    }

    // Full layout of one member (including array handling)
    constexpr LayoutInfo MemberLayout(const MemberDecl &m,
                                      std::span<const StructDecl> structs,
                                      LayoutStandard std)
    {
        LayoutInfo el = ElementLayout(m, structs, std);
        if (m.arraySize > 0) {
            if (std == LayoutStandard::STD140) {
                uint32_t mx = el.size > el.alignment ? el.size : el.alignment;
                el.stride    = AlignUp(mx, 16);
                el.alignment = el.alignment > 16u ? el.alignment : 16u;
            } else {
                el.stride = AlignUp(el.size, el.alignment);
            }
            el.size = el.stride * m.arraySize;
        }
        return el;
    }

    // Total byte size of a flat member list
    constexpr uint32_t CalculateMembersSize(std::span<const MemberDecl> members,
                                             std::span<const StructDecl> structs,
                                             LayoutStandard std)
    {
        uint32_t offset = 0;
        for (const auto &m : members) {
            LayoutInfo ml = MemberLayout(m, structs, std);
            offset = AlignUp(offset, ml.alignment);
            offset += ml.size;
        }
        return offset;
    }

    // Offset of the Nth member (0-based index)
    constexpr uint32_t CalculateMemberOffset(std::span<const MemberDecl> members,
                                              std::span<const StructDecl> structs,
                                              LayoutStandard std,
                                              uint32_t index)
    {
        uint32_t offset = 0;
        for (uint32_t i = 0; i < members.size(); ++i) {
            LayoutInfo ml = MemberLayout(members[i], structs, std);
            offset = AlignUp(offset, ml.alignment);
            if (i == index) return offset;
            offset += ml.size;
        }
        return offset; // past-end if index >= size
    }

    // Struct layout: total size with trailing alignment, max alignment
    constexpr LayoutInfo CalculateStructLayout(const StructDecl &decl,
                                               std::span<const StructDecl> structs)
    {
        LayoutStandard std = GetLayoutStandard(decl);
        uint32_t totalSize = CalculateMembersSize(decl.members, structs, std);

        uint32_t maxAlign = 0;
        for (const auto &m : decl.members) {
            LayoutInfo ml = MemberLayout(m, structs, std);
            if (ml.alignment > maxAlign) maxAlign = ml.alignment;
        }

        if (std == LayoutStandard::STD140 && maxAlign < 16u) {
            maxAlign = 16u;
        }

        totalSize = AlignUp(totalSize, maxAlign);
        return {totalSize, maxAlign, 0, 0};
    }

    // Convenience: struct total size
    constexpr uint32_t CalculateStructSize(const StructDecl &decl,
                                            std::span<const StructDecl> structs = {})
    {
        return CalculateStructLayout(decl, structs).size;
    }

} // namespace sky::sl
