//
// Created on 2026/09/25.
//
// Benchmark suite for the replication / security paths. Run NetworkBenchmark (Release recommended) and
// compare the printed table across refactors. Sections: scale, field delta, dormancy, split, AoI,
// multi-connection, resume token. Not part of the test suite.
//

#include <network/replication/ReplicationSnapshot.h>

#include <network/NetworkSession.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

using namespace sky::net;

namespace {

    // Opaque record with revision tracking (revision == 0 means "always encode").
    struct BenchRecord : public IReplicationRecord {
        ReplicatedEntityId   entity = 0;
        ReplicationTypeId    type = 100;
        std::vector<uint8_t> state;
        uint32_t             revision = 1;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return type; }
        void Encode(std::vector<uint8_t> &out) const override { out = state; }
        FieldMask DirtyMask() const override { return 0; }
        uint32_t  Revision() const override { return revision; }
        void Apply(std::span<const uint8_t> data) override { state.assign(data.begin(), data.end()); }
    };

    // Field-based record with 8 replicated float fields.
    struct FieldBenchRecord : public IReplicationRecord {
        static constexpr uint32_t FIELD_COUNT = 8;
        ReplicatedEntityId entity = 0;
        ReplicationTypeId  type = 200;
        float              fields[FIELD_COUNT] = {};
        uint32_t           revision = 1;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return type; }
        uint32_t FieldCount() const override { return FIELD_COUNT; }
        void EncodeField(uint32_t index, std::vector<uint8_t> &out) const override
        {
            out.resize(4);
            std::memcpy(out.data(), &fields[index], 4);
        }
        void ApplyField(uint32_t index, std::span<const uint8_t> d) override
        {
            if (d.size() >= 4) {
                std::memcpy(&fields[index], d.data(), 4);
            }
        }
        void Encode(std::vector<uint8_t> &out) const override
        {
            out.resize(sizeof(fields));
            std::memcpy(out.data(), fields, sizeof(fields));
        }
        FieldMask DirtyMask() const override { return 0; }
        uint32_t  Revision() const override { return revision; }
        void Apply(std::span<const uint8_t> d) override
        {
            if (d.size() >= sizeof(fields)) {
                std::memcpy(fields, d.data(), sizeof(fields));
            }
        }
    };

    struct BenchSource : public IReplicationSource {
        std::vector<std::unique_ptr<BenchRecord>> storage;
        std::vector<BenchRecord *>                 records;
        uint32_t                                   cullEveryN = 0;

        BenchRecord *Add(ReplicatedEntityId entity, uint32_t size)
        {
            auto record = std::make_unique<BenchRecord>();
            record->entity = entity;
            record->state.assign(size, 0);
            auto *ptr = record.get();
            records.push_back(ptr);
            storage.push_back(std::move(record));
            return ptr;
        }

        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override
        {
            for (auto *record : records) {
                fn(*record);
            }
        }
        bool IsRelevantFor(const IReplicationRecord &record, ConnectionId) const override
        {
            return cullEveryN == 0 || (record.Entity() % cullEveryN) != 0;
        }
        IReplicationRecord *CreateReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
        void DestroyReplica(ReplicatedEntityId, ReplicationTypeId) override {}
        IReplicationRecord *FindReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
    };

    struct FieldBenchSource : public IReplicationSource {
        std::vector<std::unique_ptr<FieldBenchRecord>> storage;
        std::vector<FieldBenchRecord *>                 records;

        FieldBenchRecord *Add(ReplicatedEntityId entity)
        {
            auto record = std::make_unique<FieldBenchRecord>();
            record->entity = entity;
            auto *ptr = record.get();
            records.push_back(ptr);
            storage.push_back(std::move(record));
            return ptr;
        }
        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override
        {
            for (auto *record : records) {
                fn(*record);
            }
        }
        IReplicationRecord *CreateReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
        void DestroyReplica(ReplicatedEntityId, ReplicationTypeId) override {}
        IReplicationRecord *FindReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
    };

    using Clock = std::chrono::steady_clock;

    template <typename Fn>
    double TimeMs(Fn &&fn)
    {
        const auto start = Clock::now();
        fn();
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    size_t TotalBytes(const std::vector<std::vector<uint8_t>> &messages)
    {
        size_t total = 0;
        for (const auto &message : messages) {
            total += message.size();
        }
        return total;
    }

} // namespace

int main()
{
    constexpr uint32_t PAYLOAD_SIZE = 12;
    constexpr uint32_t ITERATIONS   = 2000;

    std::printf("== SkyEngine network replication benchmark ==\n");
    std::printf("config: payload=%u bytes, iterations=%u\n\n", PAYLOAD_SIZE, ITERATIONS);

    // --- 1. Scale sweep: opaque records, every entity changes each tick ---
    std::printf("[scale] opaque, all entities change each tick\n");
    std::printf("  %-10s %-14s %-14s\n", "entities", "build ms/tick", "apply ms/tick");
    for (uint32_t entityCount : {100u, 1000u, 5000u}) {
        BenchSource server;
        for (ReplicatedEntityId i = 1; i <= entityCount; ++i) {
            server.Add(i, PAYLOAD_SIZE);
        }
        ReplicationConfig config;
        config.maxMessageBytes        = 1u << 20;
        config.bandwidthBudgetBytes   = 1u << 30;
        config.maxEntitiesPerSnapshot = entityCount;

        SnapshotCodec::ConnectionState state;
        std::vector<uint8_t> message;
        const double buildMs = TimeMs([&]() {
            for (uint32_t i = 0; i < ITERATIONS; ++i) {
                server.records[i % entityCount]->state[0] = static_cast<uint8_t>(i);
                ++server.records[i % entityCount]->revision;
                SnapshotCodec::BuildState(server, state, config, i + 1, message);
            }
        });

        BenchSource source;
        for (ReplicatedEntityId i = 1; i <= entityCount; ++i) {
            source.Add(i, PAYLOAD_SIZE);
        }
        SnapshotCodec::ConnectionState applyState;
        SnapshotCodec::BuildState(source, applyState, config, 1, message);
        BenchSource client;
        const double applyMs = TimeMs([&]() {
            for (uint32_t i = 0; i < ITERATIONS; ++i) {
                SnapshotSequence sequence = 0;
                SnapshotCodec::ApplyState(client, message, sequence);
            }
        });
        std::printf("  %-10u %-14.4f %-14.4f\n", entityCount, buildMs / ITERATIONS, applyMs / ITERATIONS);
    }
    std::printf("\n");

    // --- 2. Field delta: bytes per tick vs number of changed fields ---
    std::printf("[field delta] 2000 entities, 8 fields of 4 bytes each\n");
    std::printf("  %-10s %-14s\n", "changed", "bytes/tick");
    {
        constexpr uint32_t FIELD_ENTITIES = 2000;
        for (uint32_t changedFields : {1u, 2u, 4u, 8u}) {
            FieldBenchSource server;
            for (ReplicatedEntityId i = 1; i <= FIELD_ENTITIES; ++i) {
                server.Add(i);
            }
            ReplicationConfig config;
            config.maxMessageBytes        = 1u << 20;
            config.bandwidthBudgetBytes   = 1u << 30;
            config.maxEntitiesPerSnapshot = FIELD_ENTITIES;

            SnapshotCodec::ConnectionState state;
            std::vector<std::vector<uint8_t>> messages;
            SnapshotCodec::BuildStateMessages(server, state, config, 1, INVALID_CONNECTION_ID, messages);   // baseline

            for (auto *record : server.records) {
                for (uint32_t f = 0; f < changedFields; ++f) {
                    record->fields[f] = 1.0f;
                }
                ++record->revision;
            }
            SnapshotCodec::BuildStateMessages(server, state, config, 2, INVALID_CONNECTION_ID, messages);
            std::printf("  %-10u %-14zu\n", changedFields, TotalBytes(messages));
        }
    }
    std::printf("\n");

    // --- 3. Dormancy: unchanged entities must not be encoded (revision skip) ---
    std::printf("[dormancy] 2000 unchanged, revision-tracked\n");
    {
        constexpr uint32_t DORMANT = 2000;
        BenchSource server;
        for (ReplicatedEntityId i = 1; i <= DORMANT; ++i) {
            server.Add(i, PAYLOAD_SIZE);
        }
        ReplicationConfig config;
        config.maxMessageBytes        = 1u << 20;
        config.bandwidthBudgetBytes   = 1u << 30;
        config.maxEntitiesPerSnapshot = DORMANT;

        SnapshotCodec::ConnectionState state;
        std::vector<std::vector<uint8_t>> messages;
        SnapshotCodec::BuildStateMessages(server, state, config, 1, INVALID_CONNECTION_ID, messages);
        SnapshotCodec::Acknowledge(state, 1);
        const double dormantMs = TimeMs([&]() {
            for (uint32_t i = 0; i < ITERATIONS; ++i) {
                SnapshotCodec::BuildStateMessages(server, state, config, i + 2, INVALID_CONNECTION_ID, messages);
                SnapshotCodec::Acknowledge(state, i + 2);   // steady state: the client acks each tick
            }
        });
        std::printf("  build %.4f ms/tick, sent %zu bytes/tick\n\n", dormantMs / ITERATIONS, TotalBytes(messages));
    }

    // --- 4. Split: message count under a small per-message cap ---
    std::printf("[split] 2000 entities, all change, small per-message cap\n");
    std::printf("  %-14s %-12s %-12s\n", "maxMessageBytes", "messages", "bytes/tick");
    {
        constexpr uint32_t SPLIT_ENTITIES = 2000;
        for (uint32_t cap : {256u, 512u, 1200u}) {
            BenchSource server;
            for (ReplicatedEntityId i = 1; i <= SPLIT_ENTITIES; ++i) {
                server.Add(i, PAYLOAD_SIZE);
            }
            ReplicationConfig config;
            config.maxMessageBytes        = cap;
            config.bandwidthBudgetBytes   = 1u << 30;
            config.maxEntitiesPerSnapshot = SPLIT_ENTITIES;

            SnapshotCodec::ConnectionState state;
            std::vector<std::vector<uint8_t>> messages;
            SnapshotCodec::BuildStateMessages(server, state, config, 1, INVALID_CONNECTION_ID, messages);
            std::printf("  %-14u %-12zu %-12zu\n", cap, messages.size(), TotalBytes(messages));
        }
    }
    std::printf("\n");

    // --- 5. AoI: per-connection culling ---
    std::printf("[AoI] 2000 entities, cull entities divisible by N\n");
    std::printf("  %-14s %-14s %-14s\n", "cullEveryN", "culled", "bytes/tick");
    {
        constexpr uint32_t AOI_ENTITIES = 2000;
        for (uint32_t cull : {2u, 4u, 8u}) {
            BenchSource server;
            for (ReplicatedEntityId i = 1; i <= AOI_ENTITIES; ++i) {
                server.Add(i, PAYLOAD_SIZE);
            }
            server.cullEveryN = cull;
            ReplicationConfig config;
            config.maxMessageBytes        = 1u << 20;
            config.bandwidthBudgetBytes   = 1u << 30;
            config.maxEntitiesPerSnapshot = AOI_ENTITIES;

            SnapshotCodec::ConnectionState state;
            std::vector<std::vector<uint8_t>> messages;
            SnapshotCodec::BuildStateMessages(server, state, config, 1, ConnectionId{1, 1}, messages);
            std::printf("  %-14u %-14u %-14zu\n", cull, AOI_ENTITIES / cull, TotalBytes(messages));
        }
    }
    std::printf("\n");

    // --- 6. Multi-connection aggregate: with and without the shared encoding cache ---
    std::printf("[multi-connection] 1000 entities, all change, per tick\n");
    std::printf("  %-12s %-16s %-16s %-10s\n", "connections", "no-cache ms/tick", "cache ms/tick", "speedup");
    {
        constexpr uint32_t MC_ENTITIES = 1000;
        for (uint32_t connectionCount : {4u, 16u, 64u}) {
            BenchSource server;
            for (ReplicatedEntityId i = 1; i <= MC_ENTITIES; ++i) {
                server.Add(i, PAYLOAD_SIZE);
            }
            ReplicationConfig config;
            config.maxMessageBytes        = 1u << 20;
            config.bandwidthBudgetBytes   = 1u << 30;
            config.maxEntitiesPerSnapshot = MC_ENTITIES;

            auto run = [&](bool useCache) -> double {
                std::vector<SnapshotCodec::ConnectionState> states(connectionCount);
                EncodedRecordCache cache;
                std::vector<std::vector<uint8_t>> messages;
                const double ms = TimeMs([&]() {
                    for (uint32_t i = 0; i < 200; ++i) {
                        for (auto *record : server.records) {
                            record->state[0] = static_cast<uint8_t>(i);
                            ++record->revision;
                        }
                        for (uint32_t c = 0; c < connectionCount; ++c) {
                            SnapshotCodec::BuildStateMessages(server, states[c], config, i + 1, ConnectionId{c + 1, 1},
                                                              messages, useCache ? &cache : nullptr);
                        }
                    }
                });
                return ms / 200.0;
            };

            const double noCache  = run(false);
            const double withCache = run(true);
            std::printf("  %-12u %-16.3f %-16.3f %-10.2f\n", connectionCount, noCache, withCache,
                        noCache / withCache);
        }
    }
    std::printf("\n");

    // --- 7. Resume token: HMAC-SHA256 issue + verify ---
    std::printf("[resume token] HMAC-SHA256\n");
    {
        ResumeTokenCodec codec(0x0123456789ABCDEFull);
        constexpr uint32_t TOKEN_ITERATIONS = 100000;
        const double issueMs = TimeMs([&]() {
            for (uint32_t i = 0; i < TOKEN_ITERATIONS; ++i) {
                volatile ResumeToken token = codec.Issue(SessionId{7}, i, 60000, i);
                (void)token;
            }
        });
        const ResumeToken token = codec.Issue(SessionId{7}, 0, 60000, 1);
        const double verifyMs = TimeMs([&]() {
            for (uint32_t i = 0; i < TOKEN_ITERATIONS; ++i) {
                volatile bool ok = codec.Verify(token, 1000);
                (void)ok;
            }
        });
        std::printf("  issue %.0f/s, verify %.0f/s\n", TOKEN_ITERATIONS * 1000.0 / issueMs,
                    TOKEN_ITERATIONS * 1000.0 / verifyMs);
    }

    return 0;
}
