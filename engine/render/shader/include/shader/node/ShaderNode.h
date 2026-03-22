//
// Created by blues on 2025/3/28.
//

#pragma once

#include <shader/node/ShaderDataType.h>
#include <rhi/Core.h>
#include <span>

namespace sky::sl {

    // ── Resource Type Tag ──
    enum class ResourceType : uint8_t {
        CONSTANT_BUFFER,
        STRUCTURED_BUFFER,
        TEXTURE,
        SAMPLER
    };

    // ── Flat resource declaration — constexpr-friendly ──
    struct ResourceDecl {
        ResourceType          type       = ResourceType::CONSTANT_BUFFER;
        std::string_view      name;
        uint32_t              binding    = UINT32_MAX; // UINT32_MAX = auto
        rhi::ShaderStageFlags visibility = {};

        // ConstantBuffer
        std::span<const MemberDecl> members = {};
        bool                        dynamic = false;

        // StructuredBuffer
        ValueType             elementType      = {};
        std::string_view      elementStructRef;
        bool                  readOnly = true;

        // Texture
        TextureType           texType  = TextureType::TEXTURE_2D;
        bool                  storage  = false;
    };

} // namespace sky::sl
