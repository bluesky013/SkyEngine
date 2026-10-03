//
// Created on 2026/10/03.
//

#pragma once

#include <network/replication/IReplicationSource.h>

#include <framework/world/World.h>
#include <framework/serialization/SerializationContext.h>

#include <functional>
#include <span>
#include <unordered_map>
#include <vector>

namespace sky::net {

    // Set while the replication source applies state, so component mutators can skip gameplay
    // side effects (for example reconstructing backend objects) during a replicated apply.
    bool IsApplyingReplication();

    class ReplicationApplyScope {
    public:
        ReplicationApplyScope();
        ~ReplicationApplyScope();
        ReplicationApplyScope(const ReplicationApplyScope &) = delete;
        ReplicationApplyScope &operator=(const ReplicationApplyScope &) = delete;

    private:
        bool previous;
    };

    // One reflected member selected for replication.
    struct ReplicatedMember {
        const TypeInfoRT      *info    = nullptr;
        serialize::SetterFn    setter  = nullptr;
        serialize::GetterConstFn getter = nullptr;
    };

    // A component type and its replicated members (from REPLICATED reflection flags).
    struct ActorReplicationType {
        Uuid                           componentTypeId;
        ReplicationTypeId              type = 0;
        std::vector<ReplicatedMember>  members;

        // Per-iteration bucket of live components of this type (dense, deterministic order).
        std::vector<ComponentBase*>    bucket;
    };

    // Record returned through the seam; reused per iteration/call.
    class ActorReplicationRecord : public IReplicationRecord {
    public:
        const ActorReplicationType *type      = nullptr;
        ComponentBase              *component = nullptr;
        ReplicatedEntityId          entity    = 0;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return type != nullptr ? type->type : 0; }

        void Encode(std::vector<uint8_t> &out) const override;
        void Apply(std::span<const uint8_t> data) override;

        uint32_t FieldCount() const override
        {
            return type != nullptr ? static_cast<uint32_t>(type->members.size()) : 0u;
        }
        void EncodeField(uint32_t index, std::vector<uint8_t> &out) const override;
        void ApplyField(uint32_t index, std::span<const uint8_t> data) override;
    };

    // Replication source over the framework Actor/Component model. Field selection comes from the
    // component's reflection metadata (REPLICATED flag); encoding reuses reflection + binary archive.
    // Maintains an O(1) entity-id -> actor index driven by world attach/detach events.
    class ActorReplicationSource : public IReplicationSource, public IWorldEvent {
    public:
        explicit ActorReplicationSource(World &world);
        ~ActorReplicationSource() override;

        void RegisterComponent(const Uuid &componentTypeId, ReplicationTypeId replicationTypeId);

        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override;
        IReplicationRecord *CreateReplica(ReplicatedEntityId entity, ReplicationTypeId type) override;
        void DestroyReplica(ReplicatedEntityId entity, ReplicationTypeId type) override;
        IReplicationRecord *FindReplica(ReplicatedEntityId entity, ReplicationTypeId type) override;

        // IWorldEvent: keep the entity index in sync with the world.
        void OnActorAttached(Actor *actor) override;
        void OnActorDetached(Actor *actor) override;

        // Deterministic 64-bit identity for an actor's uuid; the host uses the same mapping.
        static ReplicatedEntityId EntityIdFor(const Uuid &id);

    private:
        ActorReplicationType *FindType(ReplicationTypeId type);
        Actor *FindActor(ReplicatedEntityId entity) const;

        World &world;
        std::unordered_map<ReplicatedEntityId, Actor*> actorIndex;
        std::vector<ActorReplicationType> types;
        std::unordered_map<ReplicationTypeId, size_t> typeIndex;
        std::unordered_map<Uuid, size_t> componentTypeLookup;
        ActorReplicationRecord scratch;
    };

} // namespace sky::net
