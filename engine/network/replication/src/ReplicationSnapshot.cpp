//
// Created on 2026/09/25.
//

#include <network/replication/ReplicationSnapshot.h>

#include <algorithm>

namespace sky::net {

    void SnapshotCodec::BuildState(IReplicationSource &source, ConnectionState &state, const ReplicationConfig &config,
                                   SnapshotSequence sequence, std::vector<uint8_t> &out)
    {
        std::vector<std::vector<uint8_t>> messages;
        BuildStateMessages(source, state, config, sequence, INVALID_CONNECTION_ID, messages);
        out = messages.empty() ? std::vector<uint8_t>{} : std::move(messages.front());
    }

    void SnapshotCodec::BuildStateMessages(IReplicationSource &source, ConnectionState &state,
                                           const ReplicationConfig &config, SnapshotSequence firstSequence,
                                           ConnectionId viewer, std::vector<std::vector<uint8_t>> &outMessages,
                                           EncodedRecordCache *cache)
    {
        struct Entry {
            ReplicatedEntityId entity = 0;
            ReplicationTypeId  type = 0;
            float              priority = 0.0f;
            FieldMask          dirty = 0;
            uint32_t           fieldCount = 0;
            uint32_t           revision = 0;
            // Cached bytes (stable across cache insertions) or locally owned bytes.
            const std::vector<uint8_t> *full = nullptr;
            const std::vector<std::vector<uint8_t>> *fields = nullptr;
            std::vector<uint8_t> ownedFull;
            std::vector<std::vector<uint8_t>> ownedFields;
        };
        const bool repair = !state.hasBaseline || state.ticksSinceAck > config.baselineRepairTicks;

        std::vector<Entry> entries;
        // Encode during iteration: the seam only guarantees the record reference for the callback. When a
        // shared encoding cache is supplied, a record is encoded once and reused by every connection.
        source.ForEachRecord([&](IReplicationRecord &record) {
            if (!source.IsRelevantFor(record, viewer)) {
                return;
            }
            const ReplicatedEntityId entity = record.Entity();
            const ReplicationTypeId  type   = record.Type();
            const uint64_t key = EncodedRecordCache::MakeKey(entity, type);
            const uint32_t revision = record.Revision();
            const bool trackable = revision != 0;

            if (!repair && trackable) {
                auto it = state.baselineRevision.find(key);
                if (it != state.baselineRevision.end() && it->second == revision) {
                    return;   // unchanged since baseline: skip encoding
                }
            }

            Entry entry;
            entry.entity     = entity;
            entry.type       = type;
            entry.priority   = source.PriorityFor(record, viewer);
            entry.dirty      = record.DirtyMask();
            entry.fieldCount = record.FieldCount();
            entry.revision   = revision;

            if (cache != nullptr && trackable) {
                EncodedRecord *cached = cache->Find(key, revision);
                if (cached == nullptr) {
                    cached = &cache->Create(key, revision);
                    if (entry.fieldCount > 0) {
                        cached->fields.resize(entry.fieldCount);
                        for (uint32_t f = 0; f < entry.fieldCount; ++f) {
                            record.EncodeField(f, cached->fields[f]);
                        }
                    } else {
                        record.Encode(cached->full);
                    }
                }
                entry.full   = &cached->full;
                entry.fields = &cached->fields;
            } else if (entry.fieldCount > 0) {
                entry.ownedFields.resize(entry.fieldCount);
                for (uint32_t f = 0; f < entry.fieldCount; ++f) {
                    record.EncodeField(f, entry.ownedFields[f]);
                }
            } else {
                record.Encode(entry.ownedFull);
            }
            entries.push_back(std::move(entry));
        });

        // Deterministic order: priority desc, then stable entity id asc, then type asc.
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            if (a.priority != b.priority) {
                return a.priority > b.priority;
            }
            if (a.entity != b.entity) {
                return a.entity < b.entity;
            }
            return a.type < b.type;
        });

        const auto fullOf = [](const Entry &e) -> const std::vector<uint8_t> & {
            return e.full != nullptr ? *e.full : e.ownedFull;
        };
        const auto fieldsOf = [](const Entry &e) -> const std::vector<std::vector<uint8_t>> & {
            return e.fields != nullptr ? *e.fields : e.ownedFields;
        };

        outMessages.clear();
        uint32_t emitted = 0;
        uint32_t totalBytes = 0;
        uint32_t messageSequence = firstSequence;
        uint32_t currentCount = 0;

        auto beginMessage = [&]() {
            outMessages.emplace_back();
            std::vector<uint8_t> &msg = outMessages.back();
            ByteWriter writer(msg);
            writer.U8(static_cast<uint8_t>(ReplicationMessage::Snapshot));
            writer.U8(repair ? 1u : 0u);   // full/repair flag
            writer.U32(messageSequence);
            writer.U16(0);
        };
        auto finishMessage = [&]() {
            std::vector<uint8_t> &msg = outMessages.back();
            // header: u8 type(0) + u8 flags(1) + u32 sequence(2..5) + u16 count(6..7)
            msg[6] = static_cast<uint8_t>(currentCount & 0xFFu);
            msg[7] = static_cast<uint8_t>((currentCount >> 8u) & 0xFFu);
        };

        beginMessage();
        for (const auto &entry : entries) {
            if (emitted >= config.maxEntitiesPerSnapshot || totalBytes >= config.bandwidthBudgetBytes) {
                break;
            }
            const uint64_t key = EncodedRecordCache::MakeKey(entry.entity, entry.type);

            std::vector<uint8_t> payload;
            FieldMask mask = 0;
            if (entry.fieldCount > 0) {
                const auto &fields = fieldsOf(entry);
                auto &base = state.fieldBaseline[key];
                if (base.size() != entry.fieldCount) {
                    base.assign(entry.fieldCount, {});
                }
                if (repair) {
                    mask = entry.fieldCount >= 64 ? ~static_cast<FieldMask>(0)
                                                  : ((static_cast<FieldMask>(1) << entry.fieldCount) - 1);
                } else {
                    for (uint32_t f = 0; f < entry.fieldCount; ++f) {
                        if (base[f] != fields[f]) {
                            mask |= (static_cast<FieldMask>(1) << f);
                        }
                    }
                }
                if (mask == 0) {
                    continue;
                }
                for (uint32_t f = 0; f < entry.fieldCount; ++f) {
                    if ((mask >> f) & 1u) {
                        payload.push_back(static_cast<uint8_t>(fields[f].size() & 0xFFu));
                        payload.push_back(static_cast<uint8_t>((fields[f].size() >> 8u) & 0xFFu));
                        payload.insert(payload.end(), fields[f].begin(), fields[f].end());
                    }
                }
            } else {
                const auto &full = fullOf(entry);
                auto it = state.baseline.find(key);
                const bool changed = repair || it == state.baseline.end() || it->second != full;
                if (!changed) {
                    continue;
                }
                mask = repair ? ~static_cast<FieldMask>(0) : entry.dirty;
                payload.reserve(2 + full.size());
                payload.push_back(static_cast<uint8_t>(full.size() & 0xFFu));
                payload.push_back(static_cast<uint8_t>((full.size() >> 8u) & 0xFFu));
                payload.insert(payload.end(), full.begin(), full.end());
            }

            const uint32_t recordSize = static_cast<uint32_t>(8 + 4 + 8 + payload.size());
            if (currentCount > 0 && outMessages.back().size() + recordSize > config.maxMessageBytes) {
                finishMessage();
                ++messageSequence;
                beginMessage();
                currentCount = 0;
            }

            std::vector<uint8_t> &msg = outMessages.back();
            ByteWriter writer(msg);
            writer.U64(entry.entity);
            writer.U32(entry.type);
            writer.U64(mask);
            writer.Bytes(payload);

            if (entry.fieldCount > 0) {
                const auto &fields = fieldsOf(entry);
                auto &base = state.fieldBaseline[key];
                for (uint32_t f = 0; f < entry.fieldCount; ++f) {
                    if ((mask >> f) & 1u) {
                        base[f] = fields[f];
                    }
                }
            } else {
                state.baseline[key] = fullOf(entry);
            }
            state.baselineRevision[key] = entry.revision;
            ++currentCount;
            ++emitted;
            totalBytes += recordSize;
        }
        finishMessage();

        state.hasBaseline  = true;
        state.lastSent     = messageSequence;
        state.ticksSinceAck++;
    }

    bool SnapshotCodec::ApplyState(IReplicationSource &source, std::span<const uint8_t> data, SnapshotSequence &sequenceOut)
    {
        ByteReader reader(data);
        uint8_t messageType = 0;
        if (!reader.U8(messageType) || static_cast<ReplicationMessage>(messageType) != ReplicationMessage::Snapshot) {
            return false;
        }
        uint8_t flags = 0;
        uint32_t sequence = 0;
        uint16_t count = 0;
        if (!reader.U8(flags) || !reader.U32(sequence) || !reader.U16(count)) {
            return false;
        }

        for (uint16_t i = 0; i < count; ++i) {
            uint64_t entity = 0;
            uint32_t typeId = 0;
            uint64_t mask = 0;
            if (!reader.U64(entity) || !reader.U32(typeId) || !reader.U64(mask)) {
                return false;
            }
            IReplicationRecord *record = source.FindReplica(entity, typeId);
            if (record == nullptr) {
                record = source.CreateReplica(entity, typeId);
            }

            const uint32_t fieldCount = record != nullptr ? record->FieldCount() : 0;
            if (fieldCount > 0) {
                for (uint32_t f = 0; f < fieldCount; ++f) {
                    if ((mask >> f) & 1u) {
                        uint16_t size = 0;
                        if (!reader.U16(size)) {
                            return false;
                        }
                        std::span<const uint8_t> fieldData;
                        if (!reader.Bytes(size, fieldData)) {
                            return false;
                        }
                        record->ApplyField(f, fieldData);
                    }
                }
            } else {
                uint16_t size = 0;
                if (!reader.U16(size)) {
                    return false;
                }
                std::span<const uint8_t> payload;
                if (!reader.Bytes(size, payload)) {
                    return false;
                }
                if (record != nullptr) {
                    record->Apply(payload);
                }
            }
        }

        sequenceOut = sequence;
        return reader.Ok();
    }

    bool SnapshotCodec::ParseState(std::span<const uint8_t> data, SnapshotSequence &sequenceOut, bool &fullOut)
    {
        ByteReader reader(data);
        uint8_t messageType = 0;
        uint8_t flags = 0;
        uint32_t sequence = 0;
        if (!reader.U8(messageType) || static_cast<ReplicationMessage>(messageType) != ReplicationMessage::Snapshot) {
            return false;
        }
        if (!reader.U8(flags) || !reader.U32(sequence)) {
            return false;
        }
        sequenceOut = sequence;
        fullOut = (flags & 1u) != 0;
        return true;
    }

    void SnapshotCodec::Acknowledge(ConnectionState &state, SnapshotSequence sequence)
    {
        if (sequence >= state.lastAck) {
            state.lastAck = sequence;
        }
        state.ticksSinceAck = 0;
    }

    void SnapshotCodec::BuildSpawn(ReplicatedEntityId entity, ReplicationTypeId type, std::span<const uint8_t> full,
                                   std::vector<uint8_t> &out)
    {
        out.clear();
        ByteWriter writer(out);
        writer.U8(static_cast<uint8_t>(ReplicationMessage::Spawn));
        writer.U64(entity);
        writer.U32(type);
        writer.U16(static_cast<uint16_t>(full.size()));
        writer.Bytes(full);
    }

    void SnapshotCodec::BuildDespawn(ReplicatedEntityId entity, ReplicationTypeId type, std::vector<uint8_t> &out)
    {
        out.clear();
        ByteWriter writer(out);
        writer.U8(static_cast<uint8_t>(ReplicationMessage::Despawn));
        writer.U64(entity);
        writer.U32(type);
    }

    void SnapshotCodec::BuildAck(SnapshotSequence sequence, std::vector<uint8_t> &out)
    {
        out.clear();
        ByteWriter writer(out);
        writer.U8(static_cast<uint8_t>(ReplicationMessage::Ack));
        writer.U32(sequence);
    }

    bool SnapshotCodec::ParseEvent(std::span<const uint8_t> data, ReplicationMessage &typeOut, ReplicatedEntityId &entityOut,
                                   ReplicationTypeId &typeIdOut, std::span<const uint8_t> &payloadOut)
    {
        ByteReader reader(data);
        uint8_t messageType = 0;
        uint64_t entity = 0;
        uint32_t typeId = 0;
        if (!reader.U8(messageType)) {
            return false;
        }
        typeOut = static_cast<ReplicationMessage>(messageType);
        if (typeOut == ReplicationMessage::Despawn) {
            if (!reader.U64(entity) || !reader.U32(typeId)) {
                return false;
            }
            entityOut = entity;
            typeIdOut = typeId;
            payloadOut = {};
            return true;
        }
        if (typeOut == ReplicationMessage::Spawn) {
            uint16_t size = 0;
            if (!reader.U64(entity) || !reader.U32(typeId) || !reader.U16(size)) {
                return false;
            }
            if (!reader.Bytes(size, payloadOut)) {
                return false;
            }
            entityOut = entity;
            typeIdOut = typeId;
            return true;
        }
        return false;
    }

    bool SnapshotCodec::ParseAck(std::span<const uint8_t> data, SnapshotSequence &sequenceOut)
    {
        ByteReader reader(data);
        uint8_t messageType = 0;
        uint32_t sequence = 0;
        if (!reader.U8(messageType) || static_cast<ReplicationMessage>(messageType) != ReplicationMessage::Ack) {
            return false;
        }
        if (!reader.U32(sequence)) {
            return false;
        }
        sequenceOut = sequence;
        return true;
    }

} // namespace sky::net
