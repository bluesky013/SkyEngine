//
// Created on 2026/10/03.
//

#include <network/replication/ReplicationSnapshot.h>
#include <network/world/ActorReplicationSource.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/Actor.h>
#include <framework/world/Component.h>

#include <gtest/gtest.h>

#include <memory>

using namespace sky;
using namespace sky::net;

namespace {

    struct ReplData {
        float x      = 0.f;
        float y      = 0.f;
        float hidden = 0.f;
    };

    class ReplComponent : public ComponentAdaptor<ReplData> {
    public:
        COMPONENT_RUNTIME_INFO(ReplComponent)

        static int  SetterCalls;
        static bool LastApplyGuard;

        static void Reflect(SerializationContext *context)
        {
            context->Register<ReplData>("ReplTestData").Member<&ReplData::x>("x").Member<&ReplData::y>("y").Member<&ReplData::hidden>("hidden");

            REGISTER_BEGIN(ReplComponent, context)
            REGISTER_MEMBER(X, SetX, GetX)
            SET_REPLICATED() REGISTER_MEMBER(Y, SetY, GetY) SET_REPLICATED() REGISTER_MEMBER(Hidden, SetHidden, GetHidden);
        }

        void SetX(float v)
        {
            data.x = v;
            ++SetterCalls;
            LastApplyGuard = IsApplyingReplication();
        }
        float GetX() const
        {
            return data.x;
        }

        void SetY(float v)
        {
            data.y = v;
        }
        float GetY() const
        {
            return data.y;
        }

        void SetHidden(float v)
        {
            data.hidden = v;
        }
        float GetHidden() const
        {
            return data.hidden;
        }
    };

    int  ReplComponent::SetterCalls    = 0;
    bool ReplComponent::LastApplyGuard = false;

    constexpr ReplicationTypeId REPL_TYPE = 77;

    // Reflection registration is process-global and asserts on duplicates; run it once.
    void EnsureReflection()
    {
        static bool done = false;
        if (!done) {
            World::Reflect(SerializationContext::Get());
            ReplComponent::Reflect(SerializationContext::Get());
            done = true;
        }
    }

} // namespace

TEST(ActorReplicationSourceTest, EncodeApplyReplicatedFields)
{
    EnsureReflection();

    std::unique_ptr<World> server(World::CreateWorld());
    std::unique_ptr<World> client(World::CreateWorld());

    const Uuid id          = Uuid::CreateWithSeed(4242);
    auto      *serverActor = server->CreateActor(id);
    auto      *serverComp  = serverActor->AddComponent<ReplComponent>();
    ASSERT_NE(serverComp, nullptr);
    serverComp->SetX(1.5f);
    serverComp->SetY(2.5f);
    serverComp->SetHidden(9.f);

    const Uuid typeId = TypeInfoObj<ReplComponent>::Get()->RtInfo()->registeredId;

    ActorReplicationSource serverSource(*server);
    serverSource.RegisterComponent(typeId, REPL_TYPE);

    std::vector<uint8_t> payload;
    ReplicatedEntityId   entity = 0;
    ReplicationTypeId    rtype  = 0;
    uint32_t             fields = 0;
    serverSource.ForEachRecord([&](IReplicationRecord &record) {
        payload.clear();
        record.Encode(payload);
        entity = record.Entity();
        rtype  = record.Type();
        fields = record.FieldCount();
    });

    EXPECT_EQ(fields, 2u); // X and Y replicated, hidden not
    EXPECT_EQ(rtype, REPL_TYPE);
    EXPECT_GT(payload.size(), 0u);

    auto                  *clientActor = client->CreateActor(id);
    ActorReplicationSource clientSource(*client);
    clientSource.RegisterComponent(typeId, REPL_TYPE);

    auto *record = clientSource.CreateReplica(entity, rtype);
    ASSERT_NE(record, nullptr);

    ReplComponent::SetterCalls    = 0;
    ReplComponent::LastApplyGuard = false;
    record->Apply(payload);

    auto *clientComp = clientActor->GetComponent<ReplComponent>();
    ASSERT_NE(clientComp, nullptr);
    EXPECT_FLOAT_EQ(clientComp->GetX(), 1.5f);
    EXPECT_FLOAT_EQ(clientComp->GetY(), 2.5f);
    EXPECT_FLOAT_EQ(clientComp->GetHidden(), 0.f); // unmarked field never replicated
    EXPECT_EQ(ReplComponent::SetterCalls, 1);
    EXPECT_TRUE(ReplComponent::LastApplyGuard); // apply guard active during the setter
}

TEST(ActorReplicationSourceTest, SnapshotLoopback)
{
    EnsureReflection();

    std::unique_ptr<World> server(World::CreateWorld());
    std::unique_ptr<World> client(World::CreateWorld());

    const Uuid id    = Uuid::CreateWithSeed(777);
    auto      *sComp = server->CreateActor(id)->AddComponent<ReplComponent>();
    ASSERT_NE(sComp, nullptr);
    sComp->SetX(3.f);
    sComp->SetY(4.f);
    sComp->SetHidden(5.f);

    const Uuid typeId = TypeInfoObj<ReplComponent>::Get()->RtInfo()->registeredId;

    ActorReplicationSource serverSource(*server);
    serverSource.RegisterComponent(typeId, REPL_TYPE);

    ReplicationConfig              config;
    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t>           message;
    SnapshotCodec::BuildState(serverSource, state, config, 1, message);
    ASSERT_GT(message.size(), 0u);

    // The host spawns the actor shell with the same identity; the source adds the replica component.
    client->CreateActor(id);

    ActorReplicationSource clientSource(*client);
    clientSource.RegisterComponent(typeId, REPL_TYPE);

    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(clientSource, message, sequence));

    auto *cComp = client->GetActorByUuid(id)->GetComponent<ReplComponent>();
    ASSERT_NE(cComp, nullptr);
    EXPECT_FLOAT_EQ(cComp->GetX(), 3.f);
    EXPECT_FLOAT_EQ(cComp->GetY(), 4.f);
    EXPECT_FLOAT_EQ(cComp->GetHidden(), 0.f); // not replicated
}

TEST(ActorReplicationSourceTest, IterationOrderIsDeterministic)
{
    EnsureReflection();

    std::unique_ptr<World> world(World::CreateWorld());

    const Uuid idA = Uuid::CreateWithSeed(11);
    const Uuid idB = Uuid::CreateWithSeed(22);
    const Uuid idC = Uuid::CreateWithSeed(33);
    world->CreateActor(idC)->AddComponent<ReplComponent>();
    world->CreateActor(idA)->AddComponent<ReplComponent>();
    world->CreateActor(idB)->AddComponent<ReplComponent>();

    const Uuid             typeId = TypeInfoObj<ReplComponent>::Get()->RtInfo()->registeredId;
    ActorReplicationSource source(*world);
    source.RegisterComponent(typeId, REPL_TYPE);

    std::vector<ReplicatedEntityId> first;
    source.ForEachRecord([&](IReplicationRecord &r) { first.push_back(r.Entity()); });

    std::vector<ReplicatedEntityId> second;
    source.ForEachRecord([&](IReplicationRecord &r) { second.push_back(r.Entity()); });
    EXPECT_EQ(first, second); // stable across calls

    // Stable relative order under removal: remaining entities keep their relative position.
    ASSERT_EQ(first.size(), 3u);
    world->GetActorByUuid(idA)->RemoveComponent<ReplComponent>();

    std::vector<ReplicatedEntityId> expected;
    for (auto entity : first) {
        if (entity != ActorReplicationSource::EntityIdFor(idA)) {
            expected.push_back(entity);
        }
    }

    std::vector<ReplicatedEntityId> after;
    source.ForEachRecord([&](IReplicationRecord &r) { after.push_back(r.Entity()); });
    EXPECT_EQ(after, expected);
}

TEST(ActorReplicationSourceTest, IndexTracksAttachDetach)
{
    EnsureReflection();

    std::unique_ptr<World> world(World::CreateWorld());
    const Uuid             typeId = TypeInfoObj<ReplComponent>::Get()->RtInfo()->registeredId;

    // Constructed before any actor exists; the index is driven by world events.
    ActorReplicationSource source(*world);
    source.RegisterComponent(typeId, REPL_TYPE);

    const Uuid id    = Uuid::CreateWithSeed(555);
    auto      *actor = world->CreateActor(id);
    actor->AddComponent<ReplComponent>();

    const auto entity = ActorReplicationSource::EntityIdFor(id);
    ASSERT_NE(source.CreateReplica(entity, REPL_TYPE), nullptr);

    // Detach drops the actor from the index; re-attach restores it.
    auto owned = world->DetachFromWorld(actor);
    EXPECT_EQ(source.FindReplica(entity, REPL_TYPE), nullptr);

    world->AttachToWorld(std::move(owned));
    EXPECT_NE(source.FindReplica(entity, REPL_TYPE), nullptr);
}

TEST(ActorReplicationSourceTest, FieldEncodeApply)
{
    EnsureReflection();

    std::unique_ptr<World> world(World::CreateWorld());
    const Uuid             id    = Uuid::CreateWithSeed(99);
    auto                  *actor = world->CreateActor(id);
    auto                  *comp  = actor->AddComponent<ReplComponent>();
    comp->SetX(1.f);
    comp->SetY(2.f);

    const Uuid             typeId = TypeInfoObj<ReplComponent>::Get()->RtInfo()->registeredId;
    ActorReplicationSource source(*world);
    source.RegisterComponent(typeId, REPL_TYPE);

    IReplicationRecord *record = nullptr;
    source.ForEachRecord([&](IReplicationRecord &r) { record = &r; });
    ASSERT_NE(record, nullptr);

    std::vector<uint8_t> fieldX;
    record->EncodeField(0, fieldX);
    comp->SetX(0.f);
    record->ApplyField(0, fieldX);
    EXPECT_FLOAT_EQ(comp->GetX(), 1.f);
    EXPECT_FLOAT_EQ(comp->GetY(), 2.f); // untouched by field 0

    std::vector<uint8_t> fieldY;
    record->EncodeField(1, fieldY);
    comp->SetY(0.f);
    record->ApplyField(1, fieldY);
    EXPECT_FLOAT_EQ(comp->GetY(), 2.f);
}
