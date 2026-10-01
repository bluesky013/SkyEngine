//
// Created on 2026/09/25.
//

#pragma once

#include <network/replication/ReplicationTypes.h>

#include <network/ConnectionId.h>

#include <functional>
#include <span>
#include <vector>

namespace sky::net {

    // One replicated component instance owned by the simulation. The algorithm never inspects fields;
    // it moves opaque encoded state and a changed-field mask so the source can stay Framework-free.
    class IReplicationRecord {
    public:
        virtual ~IReplicationRecord() = default;

        virtual ReplicatedEntityId Entity() const = 0;
        virtual ReplicationTypeId  Type() const = 0;

        // Encode the full component state.
        virtual void Encode(std::vector<uint8_t> &out) const = 0;

        // Fields changed since the last tick (server side); 0 when unused.
        virtual FieldMask DirtyMask() const { return 0; }

        // Monotonic counter bumped on mutation. When non-zero and unchanged since a connection's baseline,
        // the algorithm skips encoding this record entirely (a CPU optimization on top of delta/dormancy).
        // 0 disables revision tracking (the record is always encoded).
        virtual uint32_t Revision() const { return 0; }

        // Apply encoded full state (client side).
        virtual void Apply(std::span<const uint8_t> data) = 0;

        // Field-level delta support. A record with FieldCount() > 0 participates in masked field delta;
        // a record with 0 uses the opaque whole-record path.
        virtual uint32_t FieldCount() const { return 0; }
        virtual void     EncodeField(uint32_t index, std::vector<uint8_t> &out) const { (void)index; (void)out; }
        virtual void     ApplyField(uint32_t index, std::span<const uint8_t> data) { (void)index; (void)data; }
    };

    // Data-driven source seam. Iteration is dense and batched and may be assumed to be in a
    // deterministic order stabilized by stable entity id.
    class IReplicationSource {
    public:
        virtual ~IReplicationSource() = default;

        virtual void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) = 0;

        // Interest management hooks. Higher priority is sent first under budget pressure.
        virtual float Priority(const IReplicationRecord &record) const { (void)record; return 1.0f; }
        virtual bool  IsRelevant(const IReplicationRecord &record) const { (void)record; return true; }

        // Per-connection interest/priority. Default to the connection-agnostic versions; override for
        // per-client culling (area of interest).
        virtual bool IsRelevantFor(const IReplicationRecord &record, ConnectionId connection) const
        {
            (void)connection;
            return IsRelevant(record);
        }
        virtual float PriorityFor(const IReplicationRecord &record, ConnectionId connection) const
        {
            (void)connection;
            return Priority(record);
        }

        // Client replica lifecycle.
        virtual IReplicationRecord *CreateReplica(ReplicatedEntityId entity, ReplicationTypeId type) = 0;
        virtual void                DestroyReplica(ReplicatedEntityId entity, ReplicationTypeId type) = 0;
        virtual IReplicationRecord *FindReplica(ReplicatedEntityId entity, ReplicationTypeId type) = 0;
    };

} // namespace sky::net
