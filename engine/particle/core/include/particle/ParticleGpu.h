#pragma once

#include <cstdint>

namespace sky::particle {

    enum class ParticleGpuPass : uint8_t { RESET, SPAWN, SIMULATE, COMPACT, SORT, RENDER, END };

    struct ParticleGpuCapabilities {
        bool compute          = false;
        bool indirectDraw     = false;
        bool indirectDispatch = false;
        bool storageBuffers   = false;
        bool sort             = false;
    };

    // Persistent per-emitter GPU state; populated by the future GPU backend.
    struct ParticleGpuEmitterLayout {
        uint32_t capacity      = 0;
        uint32_t stateStride   = 0;
        uint32_t freeListSize  = 0;
        uint32_t aliveListSize = 0;
    };

    // Stable contract for the GPU backend added by add-particle-gpu.
    struct ParticleGpuFrameDesc {
        ParticleGpuPass pass    = ParticleGpuPass::RESET;
        uint32_t        groupsX = 1, groupsY = 1, groupsZ = 1;
        bool            indirect = false;
    };

} // namespace sky::particle
