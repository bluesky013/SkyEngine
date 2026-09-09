//
// ECS entity id: 24 bit index | 8 bit generation.
//

#pragma once

#include <core/platform/Platform.h>
#include <cstdint>

namespace sky {

    using EntityId = uint32_t;

    constexpr uint32_t ECS_INDEX_BITS     = 24;
    constexpr uint32_t ECS_INDEX_MASK     = (1u << ECS_INDEX_BITS) - 1u;
    constexpr uint32_t ECS_GENERATION_BITS = 8;
    constexpr EntityId INVALID_ENTITY     = 0xFFFFFFFFu;

    FORCEINLINE EntityId MakeEntityId(uint32_t index, uint32_t generation)
    {
        return (index & ECS_INDEX_MASK) | ((generation & 0xFFu) << ECS_INDEX_BITS);
    }

    FORCEINLINE uint32_t GetEntityIndex(EntityId id)
    {
        return id & ECS_INDEX_MASK;
    }

    FORCEINLINE uint32_t GetEntityGeneration(EntityId id)
    {
        return id >> ECS_INDEX_BITS;
    }

    FORCEINLINE bool IsValidEntity(EntityId id)
    {
        return id != INVALID_ENTITY;
    }

} // namespace sky
