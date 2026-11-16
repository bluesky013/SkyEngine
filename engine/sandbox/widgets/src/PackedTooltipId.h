#pragma once
#include <cstdint>

namespace sky::editor {
    inline uint64_t PackTooltipId(uint32_t generation, uint32_t ordinal)
    {
        return (uint64_t(generation) << 32) | ordinal;
    }
    inline uint32_t TooltipOrdinal(uint64_t id)
    {
        return static_cast<uint32_t>(id);
    }
    inline bool TooltipGenerationMatches(uint64_t id, uint32_t generation)
    {
        return (id >> 32) == generation;
    }
} // namespace sky::editor
