//
// Created on 2026/09/22.
//
// Render-independent PVS visibility primitives: object identity, the packed
// visibility view id layout, and the fail-safe visibility bit query. Kept free
// of any render/editor dependency so it can be unit tested on its own.
//

#pragma once

#include <core/platform/Platform.h>

#include <cstdint>

namespace sky {

    using PVSObjectID = uint32_t;
    static constexpr PVSObjectID INVALID_PVS_OBJECT = 0xFFFFFF00;

    #define PVS_OBJECT_MASK_IN_BYTES_BIT 8
    #define PVS_OBJECT_INDEX_IN_BYTES_BIT 24
    static_assert((PVS_OBJECT_MASK_IN_BYTES_BIT + PVS_OBJECT_INDEX_IN_BYTES_BIT) == sizeof(PVSObjectID) * 8);

    // 0xFFFFFF is the reserved invalid object id.
    static constexpr PVSObjectID MAX_OBJECTS = (1 << 24) - 2;

    /**
     * @brief Unique identifier for bitset visit
     */
    struct PVSVisibilityViewID {
        FORCEINLINE bool IsValid() const { return (value & INVALID_PVS_OBJECT) != INVALID_PVS_OBJECT; }

        explicit PVSVisibilityViewID() : value{INVALID_PVS_OBJECT} {}

        explicit PVSVisibilityViewID(PVSObjectID objectID) : value{objectID} {}

        union {
            PVSObjectID value;
            struct {
                uint32_t maskInBytes  : PVS_OBJECT_MASK_IN_BYTES_BIT;
                uint32_t indexInBytes : PVS_OBJECT_INDEX_IN_BYTES_BIT;
            };
        };
    };

    /**
     * @brief Query the visibility bitset for an object.
     *
     * Fails safe: when the data is unavailable (null pointer), the object id is
     * not a valid object id, or the addressed byte is outside the known data
     * size, the object is reported VISIBLE rather than culled.
     *
     * @param data             per-cell visibility bitset bytes (may be null)
     * @param dataSizeInBytes  known size of @p data, or 0 when unknown
     * @param id               object id to query
     * @return true when the object should be considered visible
     */
    bool QueryPVSObjectVisible(const uint8_t *data, uint32_t dataSizeInBytes, PVSObjectID id) noexcept;

} // namespace sky
