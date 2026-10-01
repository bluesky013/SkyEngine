//
// Created on 2026/09/25.
//

#include <network/replication/ReplicationInterpolation.h>
#include <network/replication/ReplicationSnapshot.h>

#include <gtest/gtest.h>
#include <memory>

using namespace sky::net;

namespace {

    struct MockRecord : public IReplicationRecord {
        ReplicatedEntityId   entity = 0;
        std::vector<uint8_t> state;
        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return 100; }
        void Encode(std::vector<uint8_t> &out) const override { out = state; }
        FieldMask DirtyMask() const override { return 0; }
        void Apply(std::span<const uint8_t> data) override { state.assign(data.begin(), data.end()); }
    };

    struct MockSource : public IReplicationSource {
        std::vector<std::unique_ptr<MockRecord>> storage;
        std::vector<MockRecord *>                 records;

        MockRecord *Add(ReplicatedEntityId entity, std::vector<uint8_t> state)
        {
            auto record = std::make_unique<MockRecord>();
            record->entity = entity;
            record->state  = std::move(state);
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

} // namespace

TEST(ReplicationInterpolationTest, AlphaBetweenSnapshots)
{
    SnapshotInterpolationBuffer buffer;
    const uint8_t a = 0;
    const uint8_t b = 10;
    buffer.Push(0.0, std::span<const uint8_t>(&a, 1));
    buffer.Push(1.0, std::span<const uint8_t>(&b, 1));

    EXPECT_TRUE(buffer.HasPrevious());
    EXPECT_EQ(buffer.Previous()[0], 0);
    EXPECT_EQ(buffer.Current()[0], 10);
    EXPECT_DOUBLE_EQ(buffer.Alpha(0.5, 0.0), 0.5);
    EXPECT_DOUBLE_EQ(buffer.Alpha(2.0, 0.0), 1.0);   // clamped high
    EXPECT_DOUBLE_EQ(buffer.Alpha(-1.0, 0.0), 0.0);  // clamped low
}

TEST(ReplicationInterpolationTest, SingleSampleShowsCurrent)
{
    SnapshotInterpolationBuffer buffer;
    const uint8_t a = 7;
    buffer.Push(0.0, std::span<const uint8_t>(&a, 1));
    EXPECT_FALSE(buffer.HasPrevious());
    EXPECT_DOUBLE_EQ(buffer.Alpha(0.5, 0.0), 1.0);
}

TEST(ReplicationSnapshotTest, SplitsAcrossMessagesWithinCap)
{
    MockSource source;
    for (ReplicatedEntityId i = 1; i <= 6; ++i) {
        source.Add(i, std::vector<uint8_t>(8, static_cast<uint8_t>(i)));
    }

    ReplicationConfig config;
    config.maxMessageBytes     = 64;      // each record is 22 + 8 = 30 bytes
    config.bandwidthBudgetBytes = 100000;

    SnapshotCodec::ConnectionState state;
    std::vector<std::vector<uint8_t>> messages;
    SnapshotCodec::BuildStateMessages(source, state, config, 1, INVALID_CONNECTION_ID, messages);

    ASSERT_GT(messages.size(), 1u);
    for (const auto &message : messages) {
        EXPECT_LE(message.size(), 64u);
    }
}
