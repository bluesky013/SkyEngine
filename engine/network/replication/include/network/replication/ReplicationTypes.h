//
// Created on 2026/09/25.
//

#pragma once

#include <cstdint>

namespace sky::net {

    // Stable, cross-module replication type identity (ECS tag-hash TypeId or a component UUID).
    using ReplicationTypeId = uint32_t;

    // Stable replication entity identity, independent of container layout.
    using ReplicatedEntityId = uint64_t;

    using SnapshotSequence = uint32_t;

    // Bit per replicated field within one component (up to 64 fields per component).
    using FieldMask = uint64_t;
    inline constexpr uint32_t MAX_FIELD_COUNT = 64;

    // Channels reserved by the replication layer on the core host.
    inline constexpr uint8_t REPLICATION_STATE_CHANNEL = 1;
    inline constexpr uint8_t REPLICATION_EVENT_CHANNEL = 2;

    enum class ReplicationMessage : uint8_t {
        Snapshot = 1,
        Spawn    = 2,
        Despawn  = 3,
        Ack      = 4
    };

    struct ReplicationConfig {
        uint32_t tickRateHz              = 30;
        uint32_t maxEntitiesPerSnapshot  = 256;
        uint32_t bandwidthBudgetBytes    = 8192;
        uint32_t baselineRepairTicks     = 60;
        // Per-message cap; snapshots larger than this are split across messages.
        uint32_t maxMessageBytes         = 1200;
    };

} // namespace sky::net
