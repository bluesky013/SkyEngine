//
// Created by blues on 2024/9/13.
//

#pragma once

#include <core/template/Flags.h>
#include <core/util/ArrayBitFlag.h>

namespace sky {

    // constants
    static constexpr uint32_t MAX_VERTEX_BUFFER_BINDINGS = 64;

    // enums
    enum class VertexSemanticFlagBit : uint64_t {
        NONE      = 0x00,
        POSITION  = 0x01,
        UV        = 0x02,
        NORMAL    = 0x04,
        TANGENT   = 0x08,
        COLOR     = 0x10,
        JOINT     = 0x20,
        WEIGHT    = 0x40,
        INST0     = 0x80,
        INST1     = 0x100,
        INST2     = 0x200,
        INST3     = 0x400,

        HAS_SKIN = JOINT | WEIGHT,
    };
    using VertexSemanticFlags = Flags<VertexSemanticFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(VertexSemanticFlagBit)

    enum class RenderVertexFlagBit : uint32_t {
        NONE     = 0x00,
        SKIN     = 0x01,
        INSTANCE = 0x02,
    };
    using RenderVertexFlags = Flags<RenderVertexFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(RenderVertexFlagBit)

    using RenderPipelineFlags = ArrayBit<uint32_t, 64>;

#define OFFSET_OF(s, m) static_cast<uint32_t>(offsetof(s, m))

} // namespace sky