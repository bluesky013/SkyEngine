//
// Metal hard limits and binding-slot layout constants.
//

#pragma once

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    // Metal guarantees 31 buffer argument slots per stage (indices 0..30).
    inline constexpr uint32_t METAL_MAX_BUFFER_SLOTS  = 31;
    inline constexpr uint32_t METAL_MAX_TEXTURE_SLOTS = 128;
    inline constexpr uint32_t METAL_MAX_SAMPLER_SLOTS = 16;

    // Vertex input binding i uses BASE + i; reflected vertex-stage buffers must stay below BASE.
    inline constexpr uint32_t METAL_VERTEX_BUFFER_SLOT_BASE = METAL_MAX_BUFFER_SLOTS - MAX_VERTEX_BINDINGS; // 15

} // namespace sky::aurora
