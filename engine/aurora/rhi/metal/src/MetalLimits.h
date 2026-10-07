//
// Metal hard limits and binding-slot layout constants.
//

#pragma once

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    // Metal guarantees 31 buffer argument slots per stage (indices 0..30).
    inline constexpr uint32_t METAL_MAX_BUFFER_SLOTS = 31;

    // slang MSL flattens shader buffers to sequential [[buffer(N)]] starting at 0
    // (push constants own the highest used slot), so vertex buffers are bound from
    // the top of the table downwards: vertex binding i lands on slot
    // METAL_VERTEX_BUFFER_SLOT_BASE + i. Shader buffer bindings (incl. the push
    // constant slot) must stay below METAL_VERTEX_BUFFER_SLOT_BASE;
    // MetalGraphicsPipeline asserts this budget at creation time.
    inline constexpr uint32_t METAL_VERTEX_BUFFER_SLOT_BASE = METAL_MAX_BUFFER_SLOTS - MAX_VERTEX_BINDINGS; // 15

} // namespace sky::aurora
