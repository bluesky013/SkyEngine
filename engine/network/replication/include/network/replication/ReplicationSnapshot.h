//
// Created on 2026/09/25.
//

#pragma once

#include <network/replication/IReplicationSource.h>
#include <network/replication/ReplicationTypes.h>

#include <network/ConnectionId.h>
#include <network/detail/ByteCodec.h>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace sky::net {

    // Encoded bytes for one record, cached per server and validated by revision.
    struct EncodedRecord {
        uint32_t             revision = 0;
        std::vector<uint8_t> full;
        std::vector<std::vector<uint8_t>> fields;
    };

    // Per-server cache keyed by (entity,type). Enables one encode per record per tick to be reused across
    // all connections instead of encoding once per connection. Pointers to entries remain valid across
    // insertions of other keys (node-based container), so callers may hold references during a build.
    class EncodedRecordCache {
    public:
        static uint64_t MakeKey(ReplicatedEntityId entity, ReplicationTypeId type)
        {
            return (entity << 32) ^ static_cast<uint64_t>(type);
        }

        EncodedRecord *Find(uint64_t key, uint32_t revision)
        {
            auto it = entries.find(key);
            if (it != entries.end() && it->second.revision == revision) {
                return &it->second;
            }
            return nullptr;
        }

        EncodedRecord &Create(uint64_t key, uint32_t revision)
        {
            EncodedRecord &entry = entries[key];
            entry.revision = revision;
            entry.full.clear();
            entry.fields.clear();
            return entry;
        }

        void   Erase(uint64_t key) { entries.erase(key); }
        void   Clear() { entries.clear(); }
        size_t Size() const { return entries.size(); }

    private:
        std::unordered_map<uint64_t, EncodedRecord> entries;
    };

    // Encodes/decodes replication messages and manages the per-connection delta baseline.
    class SnapshotCodec {
    public:
        struct ConnectionState {
            SnapshotSequence lastAck       = 0;
            SnapshotSequence lastSent      = 0;
            uint32_t         ticksSinceAck = 0;
            bool             hasBaseline   = false;
            // key = entity<<32 | type
            std::unordered_map<uint64_t, std::vector<uint8_t>> baseline;                 // opaque records
            std::unordered_map<uint64_t, std::vector<std::vector<uint8_t>>> fieldBaseline; // field records
            std::unordered_map<uint64_t, uint32_t> baselineRevision;                       // record revisions
        };

        // Server: build a (possibly delta) snapshot message for one connection.
        static void BuildState(IReplicationSource &source, ConnectionState &state, const ReplicationConfig &config,
                               SnapshotSequence sequence, std::vector<uint8_t> &out);

        // Server: build snapshots, split across messages within the per-message byte cap. Each message
        // carries its own sequence (firstSequence, firstSequence+1, ...). `viewer` supplies per-connection
        // interest/priority.
        static void BuildStateMessages(IReplicationSource &source, ConnectionState &state, const ReplicationConfig &config,
                                       SnapshotSequence firstSequence, ConnectionId viewer,
                                       std::vector<std::vector<uint8_t>> &outMessages, EncodedRecordCache *cache = nullptr);

        // Client: apply a snapshot message, creating replicas as needed.
        static bool ApplyState(IReplicationSource &source, std::span<const uint8_t> data, SnapshotSequence &sequenceOut);

        // Client: read a snapshot header without applying (sequence + full/repair flag), for gap detection.
        static bool ParseState(std::span<const uint8_t> data, SnapshotSequence &sequenceOut, bool &fullOut);

        // Server: acknowledge a snapshot sequence, advancing the baseline.
        static void Acknowledge(ConnectionState &state, SnapshotSequence sequence);

        // Events.
        static void BuildSpawn(ReplicatedEntityId entity, ReplicationTypeId type, std::span<const uint8_t> full,
                               std::vector<uint8_t> &out);
        static void BuildDespawn(ReplicatedEntityId entity, ReplicationTypeId type, std::vector<uint8_t> &out);
        static void BuildAck(SnapshotSequence sequence, std::vector<uint8_t> &out);
        static bool ParseEvent(std::span<const uint8_t> data, ReplicationMessage &typeOut, ReplicatedEntityId &entityOut,
                               ReplicationTypeId &typeIdOut, std::span<const uint8_t> &payloadOut);
        static bool ParseAck(std::span<const uint8_t> data, SnapshotSequence &sequenceOut);
    };

} // namespace sky::net
