//
// Created on 2026/10/03.
//

#include <network/world/ActorReplicationSource.h>

#include <framework/world/Actor.h>
#include <framework/world/Component.h>
#include <framework/serialization/BinaryArchive.h>

#include <core/archive/MemoryStreamArchive.h>

#include <algorithm>
#include <cstring>

namespace sky::net {

    namespace {
        thread_local bool g_applyingReplication = false;

        // Encode a member value with a 4-byte length prefix so a whole-record buffer can be split
        // back into fields without knowing each field's serialized size up front.
        void EncodeMember(const ReplicatedMember &member, const ComponentBase *component, std::vector<uint8_t> &out)
        {
            out.clear();
            if (member.getter == nullptr || member.info == nullptr) {
                return;
            }

            Any value = member.getter(component);

            OMemoryArchive    oarchive;
            BinaryOutputArchive archive(oarchive);
            archive.SaveObject(value.Data(), member.info->registeredId);

            const uint32_t size = static_cast<uint32_t>(oarchive.Size());
            out.resize(sizeof(uint32_t) + size);
            std::memcpy(out.data(), &size, sizeof(uint32_t));
            if (size > 0) {
                std::memcpy(out.data() + sizeof(uint32_t), oarchive.Data(), size);
            }
        }

        void DecodeMember(const ReplicatedMember &member, ComponentBase *component, std::span<const uint8_t> blob)
        {
            if (member.getter == nullptr || member.setter == nullptr || member.info == nullptr) {
                return;
            }
            if (blob.size() < sizeof(uint32_t)) {
                return;
            }

            uint32_t size = 0;
            std::memcpy(&size, blob.data(), sizeof(uint32_t));
            if (blob.size() < sizeof(uint32_t) + size) {
                return;
            }

            IMemoryArchive    iarchive(blob.data() + sizeof(uint32_t), size);
            BinaryInputArchive archive(iarchive);

            Any value = member.getter(component);
            archive.LoadObject(value.Data(), member.info->registeredId);
            member.setter(component, value.Data());
        }
    } // namespace

    bool IsApplyingReplication()
    {
        return g_applyingReplication;
    }

    ReplicationApplyScope::ReplicationApplyScope() : previous(g_applyingReplication)
    {
        g_applyingReplication = true;
    }

    ReplicationApplyScope::~ReplicationApplyScope()
    {
        g_applyingReplication = previous;
    }

    void ActorReplicationRecord::Encode(std::vector<uint8_t> &out) const
    {
        out.clear();
        if (type == nullptr || component == nullptr) {
            return;
        }

        std::vector<uint8_t> field;
        for (const auto &member : type->members) {
            EncodeMember(member, component, field);
            out.insert(out.end(), field.begin(), field.end());
        }
    }

    void ActorReplicationRecord::Apply(std::span<const uint8_t> data)
    {
        if (type == nullptr || component == nullptr) {
            return;
        }

        ReplicationApplyScope scope;

        size_t offset = 0;
        for (const auto &member : type->members) {
            if (offset + sizeof(uint32_t) > data.size()) {
                break;
            }
            uint32_t size = 0;
            std::memcpy(&size, data.data() + offset, sizeof(uint32_t));
            const size_t blobSize = sizeof(uint32_t) + size;
            if (offset + blobSize > data.size()) {
                break;
            }
            DecodeMember(member, component, data.subspan(offset, blobSize));
            offset += blobSize;
        }
    }

    void ActorReplicationRecord::EncodeField(uint32_t index, std::vector<uint8_t> &out) const
    {
        out.clear();
        if (type == nullptr || component == nullptr || index >= type->members.size()) {
            return;
        }
        EncodeMember(type->members[index], component, out);
    }

    void ActorReplicationRecord::ApplyField(uint32_t index, std::span<const uint8_t> data)
    {
        if (type == nullptr || component == nullptr || index >= type->members.size()) {
            return;
        }
        ReplicationApplyScope scope;
        DecodeMember(type->members[index], component, data);
    }

    void ActorReplicationSource::RegisterComponent(const Uuid &componentTypeId, ReplicationTypeId replicationTypeId)
    {
        auto *node = SerializationContext::Get()->FindTypeById(componentTypeId);
        if (node == nullptr) {
            return;
        }

        ActorReplicationType entry;
        entry.componentTypeId = componentTypeId;
        entry.type            = replicationTypeId;

        for (const auto &[name, member] : node->members) {
            if (!IsReplicated(member)) {
                continue;
            }
            if (member.setterFn == nullptr || member.getterConstFn == nullptr) {
                continue;
            }
            entry.members.push_back(ReplicatedMember{ member.info, member.setterFn, member.getterConstFn });
        }

        typeIndex[replicationTypeId] = types.size();
        componentTypeLookup[componentTypeId] = types.size();
        types.push_back(std::move(entry));
    }

    ActorReplicationType *ActorReplicationSource::FindType(ReplicationTypeId type)
    {
        auto iter = typeIndex.find(type);
        return iter != typeIndex.end() ? &types[iter->second] : nullptr;
    }

    ActorReplicationSource::ActorReplicationSource(World &world) : world(world)
    {
        for (const auto &actor : world.GetActors()) {
            actorIndex[EntityIdFor(actor->GetUuid())] = actor.get();
        }
        WorldEvent::Connect(&world, this);
    }

    ActorReplicationSource::~ActorReplicationSource()
    {
        WorldEvent::DisConnect(this);
    }

    void ActorReplicationSource::OnActorAttached(Actor *actor)
    {
        if (actor != nullptr) {
            actorIndex[EntityIdFor(actor->GetUuid())] = actor;
        }
    }

    void ActorReplicationSource::OnActorDetached(Actor *actor)
    {
        if (actor != nullptr) {
            actorIndex.erase(EntityIdFor(actor->GetUuid()));
        }
    }

    ReplicatedEntityId ActorReplicationSource::EntityIdFor(const Uuid &id)
    {
        // Deterministic 64-bit identity derived from the full 128-bit uuid (uniform distribution);
        // both peers compute the same value, so no mapping needs to be transmitted.
        return id.word[0] ^ (id.word[1] * 0x9E3779B97F4A7C15ull);
    }

    Actor *ActorReplicationSource::FindActor(ReplicatedEntityId entity) const
    {
        auto iter = actorIndex.find(entity);
        return iter != actorIndex.end() ? iter->second : nullptr;
    }

    void ActorReplicationSource::ForEachRecord(const std::function<void(IReplicationRecord &)> &fn)
    {
        // Group replicated components by type (dense buckets), independent of actor storage layout.
        for (auto &entry : types) {
            entry.bucket.clear();
        }

        for (const auto &actor : world.GetActors()) {
            for (const auto &[storageId, component] : actor->GetComponents()) {
                auto lookup = componentTypeLookup.find(component->GetTypeId());
                if (lookup != componentTypeLookup.end()) {
                    types[lookup->second].bucket.emplace_back(component.get());
                }
            }
        }

        for (auto &entry : types) {
            std::sort(entry.bucket.begin(), entry.bucket.end(), [](const ComponentBase *a, const ComponentBase *b) {
                return a->GetActor()->GetUuid() < b->GetActor()->GetUuid();
            });

            for (auto *component : entry.bucket) {
                if (component == nullptr || component->GetActor() == nullptr) {
                    continue;
                }
                scratch.type      = &entry;
                scratch.component = component;
                scratch.entity    = EntityIdFor(component->GetActor()->GetUuid());
                fn(scratch);
            }
        }
    }

    IReplicationRecord *ActorReplicationSource::CreateReplica(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        auto *entry = FindType(type);
        if (entry == nullptr) {
            return nullptr;
        }

        auto *actor = FindActor(entity);
        if (actor == nullptr) {
            return nullptr;
        }

        auto *component = actor->GetComponent(entry->componentTypeId);
        if (component == nullptr) {
            component = actor->AddComponent(entry->componentTypeId);
        }
        if (component == nullptr) {
            return nullptr;
        }

        scratch.type      = entry;
        scratch.component = component;
        scratch.entity    = entity;
        return &scratch;
    }

    void ActorReplicationSource::DestroyReplica(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        auto *entry = FindType(type);
        if (entry == nullptr) {
            return;
        }
        if (auto *actor = FindActor(entity)) {
            actor->RemoveComponent(entry->componentTypeId);
        }
    }

    IReplicationRecord *ActorReplicationSource::FindReplica(ReplicatedEntityId entity, ReplicationTypeId type)
    {
        auto *entry = FindType(type);
        if (entry == nullptr) {
            return nullptr;
        }
        auto *actor = FindActor(entity);
        if (actor == nullptr) {
            return nullptr;
        }
        auto *component = actor->GetComponent(entry->componentTypeId);
        if (component == nullptr) {
            return nullptr;
        }

        scratch.type      = entry;
        scratch.component = component;
        scratch.entity    = entity;
        return &scratch;
    }

} // namespace sky::net
