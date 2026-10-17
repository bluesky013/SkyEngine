//
// Created by blues on 2024/5/14.
//

#include <filesystem>
#include <framework/serialization/SerializationUtil.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

using namespace sky;

struct TestComponentData {
    int   a;
    float b;
};

class TestComponent : public ComponentAdaptor<TestComponentData> {
public:
    TestComponent()           = default;
    ~TestComponent() override = default;

    COMPONENT_RUNTIME_INFO(TestComponent)

    static void Reflect(SerializationContext *context)
    {
        context->Register<TestComponentData>("TestComponentData").Member<&TestComponentData::a>("a").Member<&TestComponentData::b>("b");

        REGISTER_BEGIN(TestComponent, context)
        REGISTER_MEMBER(a, SetA, GetA) SET_REPLICATED() REGISTER_MEMBER(b, SetB, GetB);
    }

    void SetA(int a)
    {
        data.a = a;
    }
    int GetA() const
    {
        return data.a;
    }

    void SetB(float b)
    {
        data.b = b;
    }
    float GetB() const
    {
        return data.b;
    }
};

class LifecycleComponent : public ComponentBase {
public:
    COMPONENT_RUNTIME_INFO(LifecycleComponent)
    ~LifecycleComponent() override = default;

    static int AttachCount;
    static int DetachCount;

    static void Reflect(SerializationContext *context)
    {
        REGISTER_BEGIN(LifecycleComponent, context);
    }

    void OnAttachToWorld() override
    {
        ++AttachCount;
    }
    void OnDetachFromWorld() override
    {
        ++DetachCount;
    }
};

int LifecycleComponent::AttachCount = 0;
int LifecycleComponent::DetachCount = 0;

static void ExpectTranslation(const Transform &transform, float x, float y, float z)
{
    EXPECT_NEAR(transform.translation.x, x, 1e-4f);
    EXPECT_NEAR(transform.translation.y, y, 1e-4f);
    EXPECT_NEAR(transform.translation.z, z, 1e-4f);
}

class ComponentTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        auto *context = SerializationContext::Get();
        TestComponent::Reflect(context);
        TransformComponent::Reflect(context);
        LifecycleComponent::Reflect(context);
    }

    static void TearDownTestSuite()
    {
    }
};

TEST_F(ComponentTest, ActorTest)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                  &world = *pWorld;

    auto   id    = Uuid::CreateWithSeed(0);
    Actor *actor = world.CreateActor(id);

    {
        auto *comp = actor->AddComponent<TestComponent>();
        comp->SetA(1);
        comp->SetB(2.f);

        ASSERT_EQ(comp->GetA(), 1);
        ASSERT_EQ(comp->GetB(), 2.f);
    }

    {
        auto *comp = actor->GetComponent<TestComponent>();
        comp->SetA(1);
        comp->SetB(2.f);

        ASSERT_EQ(comp->GetA(), 1);
        ASSERT_EQ(comp->GetB(), 2.f);
    }

    {
        auto *comp = actor->GetComponent<TransformComponent>();
        comp->SetLocalRotationEuler(VEC3_ZERO);
        comp->SetLocalTranslation(Vector3(1.f, 2.f, 3.f));
        comp->SetLocalScale(Vector3(4.f, 5.f, 6.f));
    }

    {
        std::ofstream     stream((std::filesystem::temp_directory_path() / "ActorTest.json").string());
        OStreamArchive    streamArchive(stream);
        JsonOutputArchive archive(streamArchive);

        world.SaveJson(archive);
    }

    {
        actor->RemoveComponent<TestComponent>();
        auto *comp = actor->GetComponent<TestComponent>();
        ASSERT_EQ(comp, nullptr);

        actor->RemoveComponent(Uuid{});

        world.DetachFromWorld(actor);
    }

    {
        std::ifstream    stream((std::filesystem::temp_directory_path() / "ActorTest.json").string());
        IStreamArchive   streamArchive(stream);
        JsonInputArchive archive(streamArchive);

        world.LoadJson(archive);

        auto tActor = world.GetActorByUuid(id);
        ASSERT_NE(tActor, nullptr);

        auto *comp = tActor->GetComponent<TestComponent>();
        ASSERT_NE(tActor, nullptr);
        ASSERT_EQ(comp->GetA(), 1);
        ASSERT_EQ(comp->GetB(), 2.f);
    }
}

// TEST_F(ComponentTest, ActorHierarchy)
//{
//     {
//         std::unique_ptr<World> world(World::CreateWorld());
//
//         auto *actor1 = world->CreateActor();
//         auto *actor2 = world->CreateActor();
//         auto *actor3 = world->CreateActor();
//
//         actor3->SetParent(actor1);
//         ASSERT_EQ(actor1->GetChildren()[0], actor3);
//         ASSERT_EQ(actor2->GetChildren().size(), 0);
//         ASSERT_EQ(actor3->GetParent(), actor1);
//
//         actor3->SetParent(actor2);
//         ASSERT_EQ(actor1->GetChildren().size(), 0);
//         ASSERT_EQ(actor2->GetChildren()[0], actor3);
//         ASSERT_EQ(actor3->GetParent(), actor2);
//     }
//
//     {
//         std::unique_ptr<World> world(World::CreateWorld());
//
//         auto *actor1 = world->CreateActor();
//         auto *actor2 = world->CreateActor();
//         auto *actor3 = world->CreateActor();
//
//         actor2->SetParent(actor1);
//         actor3->SetParent(actor2);
//
//         world->DestroyActor(actor1);
//         ASSERT_EQ(actor2->GetChildren()[0], actor3);
//         ASSERT_EQ(actor2->GetParent(), world->GetRoot());
//         ASSERT_EQ(actor3->GetParent(), actor2);
//     }
//
// }

TEST_F(ComponentTest, TransformComponentTest)
{
    TransformComponent transformComponent;
    SetValue(transformComponent, "scale", Vector3(1, 1, 1));
    SetValue(transformComponent, "rotation", Vector3(0, 0, 0));
    SetValue(transformComponent, "translation", Vector3(1, 2, 3));

    const auto &data = transformComponent.GetData();
    ASSERT_EQ(data.local.scale.x, 1.f);
    ASSERT_EQ(data.local.scale.y, 1.f);
    ASSERT_EQ(data.local.scale.z, 1.f);

    ASSERT_EQ(data.local.rotation.x, 0.f);
    ASSERT_EQ(data.local.rotation.y, 0.f);
    ASSERT_EQ(data.local.rotation.z, 0.f);
    ASSERT_EQ(data.local.rotation.w, 1.f);

    ASSERT_EQ(data.local.translation.x, 1.f);
    ASSERT_EQ(data.local.translation.y, 2.f);
    ASSERT_EQ(data.local.translation.z, 3.f);
}

TEST_F(ComponentTest, RemoveComponentDetaches)
{
    LifecycleComponent::AttachCount = 0;
    LifecycleComponent::DetachCount = 0;

    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   actor = pWorld->CreateActor();

    auto *comp = actor->AddComponent<LifecycleComponent>();
    ASSERT_NE(comp, nullptr);
    ASSERT_EQ(LifecycleComponent::AttachCount, 1);

    actor->RemoveComponent<LifecycleComponent>();
    ASSERT_EQ(LifecycleComponent::DetachCount, 1);
    ASSERT_EQ(actor->GetComponent<LifecycleComponent>(), nullptr);
}

TEST_F(ComponentTest, UnknownComponentTypeIsSafe)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   actor = pWorld->CreateActor();

    ASSERT_EQ(actor->AddComponent(Uuid::Create()), nullptr);
    actor->RemoveComponent(Uuid::Create());
}

TEST_F(ComponentTest, WorldResetDetaches)
{
    LifecycleComponent::AttachCount = 0;
    LifecycleComponent::DetachCount = 0;

    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   actor = pWorld->CreateActor();
    actor->AddComponent<LifecycleComponent>();
    ASSERT_EQ(LifecycleComponent::DetachCount, 0);

    pWorld->Reset();
    ASSERT_EQ(LifecycleComponent::DetachCount, 1);
}

TEST_F(ComponentTest, DetachReattachKeepsSingleEntry)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                  *actor = pWorld->CreateActor();
    ASSERT_EQ(pWorld->GetActors().size(), 1u);

    auto owned = pWorld->DetachFromWorld(actor);
    ASSERT_NE(owned, nullptr);
    ASSERT_EQ(pWorld->GetActors().size(), 0u);

    pWorld->AttachToWorld(std::move(owned));
    ASSERT_EQ(pWorld->GetActors().size(), 1u);
}

TEST_F(ComponentTest, LocalSetDoesNotCompoundGlobal)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   parent = pWorld->CreateActor();
    auto                   child  = pWorld->CreateActor();

    auto *pt = parent->GetComponent<TransformComponent>();
    auto *ct = child->GetComponent<TransformComponent>();

    pt->SetLocalTranslation(Vector3(5.f, 0.f, 0.f));

    ct->SetLocalTranslation(Vector3(1.f, 0.f, 0.f));
    ct->SetParentPreserveLocal(pt);
    ExpectTranslation(ct->GetWorldTransform(), 6.f, 0.f, 0.f);

    ct->SetLocalTranslation(Vector3(2.f, 0.f, 0.f));
    ExpectTranslation(ct->GetWorldTransform(), 7.f, 0.f, 0.f);
}

TEST_F(ComponentTest, ParentMovePropagatesToChild)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   parent = pWorld->CreateActor();
    auto                   child  = pWorld->CreateActor();

    auto *pt = parent->GetComponent<TransformComponent>();
    auto *ct = child->GetComponent<TransformComponent>();

    ct->SetLocalTranslation(Vector3(1.f, 0.f, 0.f));
    ct->SetParentPreserveLocal(pt);

    pt->SetLocalTranslation(Vector3(10.f, 0.f, 0.f));
    ExpectTranslation(ct->GetWorldTransform(), 11.f, 0.f, 0.f);
}

TEST_F(ComponentTest, ReparentPreservesWorld)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   parent = pWorld->CreateActor();
    auto                   child  = pWorld->CreateActor();

    parent->GetComponent<TransformComponent>()->SetLocalTranslation(Vector3(10.f, 0.f, 0.f));

    auto *ct = child->GetComponent<TransformComponent>();
    ct->SetLocalTranslation(Vector3(1.f, 0.f, 0.f));
    ExpectTranslation(ct->GetWorldTransform(), 1.f, 0.f, 0.f);

    child->SetParent(parent);
    ExpectTranslation(ct->GetWorldTransform(), 1.f, 0.f, 0.f);
    ExpectTranslation(ct->GetLocalTransform(), -9.f, 0.f, 0.f);
}

TEST_F(ComponentTest, CycleIsRejected)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   a1 = pWorld->CreateActor();
    auto                   a2 = pWorld->CreateActor();

    auto *t1 = a1->GetComponent<TransformComponent>();
    auto *t2 = a2->GetComponent<TransformComponent>();

    t2->SetParentPreserveLocal(t1);
    ASSERT_EQ(t2->GetParent(), t1);

    t1->SetParentPreserveLocal(t2);
    ASSERT_EQ(t1->GetParent(), nullptr);
}

TEST_F(ComponentTest, SaveLoadPreservesHierarchy)
{
    Uuid parentId = Uuid::CreateWithSeed(101);
    Uuid childId  = Uuid::CreateWithSeed(102);
    auto path     = (std::filesystem::temp_directory_path() / "ComponentHierarchyTest.json").string();

    {
        std::unique_ptr<World> pWorld(World::CreateWorld());
        auto                   parent = pWorld->CreateActor(parentId);
        auto                   child  = pWorld->CreateActor(childId);

        parent->GetComponent<TransformComponent>()->SetLocalTranslation(Vector3(10.f, 0.f, 0.f));
        auto *ct = child->GetComponent<TransformComponent>();
        ct->SetLocalTranslation(Vector3(1.f, 0.f, 0.f));
        ct->SetParentPreserveLocal(parent->GetComponent<TransformComponent>());

        std::ofstream     stream(path);
        OStreamArchive    streamArchive(stream);
        JsonOutputArchive archive(streamArchive);
        pWorld->SaveJson(archive);
    }

    {
        std::unique_ptr<World> pWorld(World::CreateWorld());
        std::ifstream          stream(path);
        IStreamArchive         streamArchive(stream);
        JsonInputArchive       archive(streamArchive);
        pWorld->LoadJson(archive);

        auto loadedChild = pWorld->GetActorByUuid(childId);
        ASSERT_NE(loadedChild, nullptr);

        auto *ct = loadedChild->GetComponent<TransformComponent>();
        ASSERT_NE(ct, nullptr);
        ExpectTranslation(ct->GetLocalTransform(), 1.f, 0.f, 0.f);
        ExpectTranslation(ct->GetWorldTransform(), 11.f, 0.f, 0.f);
    }
}

TEST_F(ComponentTest, ReflectionReplicatedFlag)
{
    const auto &typeId     = TypeInfoObj<TestComponent>::Get()->RtInfo()->registeredId;
    const auto &dataTypeId = TypeInfoObj<TestComponentData>::Get()->RtInfo()->registeredId;

    auto *memberA = GetTypeMember("a", typeId);
    auto *memberB = GetTypeMember("b", typeId);
    ASSERT_NE(memberA, nullptr);
    ASSERT_NE(memberB, nullptr);
    EXPECT_TRUE(IsReplicated(*memberA));
    EXPECT_FALSE(IsReplicated(*memberB));

    // The flag lives on the component accessor node, not the data struct node.
    auto *dataA = GetTypeMember("a", dataTypeId);
    ASSERT_NE(dataA, nullptr);
    EXPECT_FALSE(IsReplicated(*dataA));
}

TEST_F(ComponentTest, ActorLookupConsistentAfterDetach)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    Uuid                   idA = Uuid::CreateWithSeed(201);
    Uuid                   idB = Uuid::CreateWithSeed(202);
    Uuid                   idC = Uuid::CreateWithSeed(203);

    pWorld->CreateActor(idA);
    auto b = pWorld->CreateActor(idB);
    pWorld->CreateActor(idC);

    pWorld->DetachFromWorld(b);

    ASSERT_EQ(pWorld->GetActors().size(), 2u);
    ASSERT_NE(pWorld->GetActorByUuid(idA), nullptr);
    ASSERT_EQ(pWorld->GetActorByUuid(idB), nullptr);
    ASSERT_NE(pWorld->GetActorByUuid(idC), nullptr);

    pWorld->CreateActor(idB);
    ASSERT_NE(pWorld->GetActorByUuid(idB), nullptr);
}

TEST_F(ComponentTest, DeterministicSerializationOrder)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto                   actor = pWorld->CreateActor();
    actor->AddComponent<TestComponent>();
    actor->AddComponent<LifecycleComponent>();

    std::string first;
    std::string second;
    {
        std::ostringstream stream;
        OStreamArchive     streamArchive(stream);
        JsonOutputArchive  archive(streamArchive);
        pWorld->SaveJson(archive);
        first = stream.str();
    }
    {
        std::ostringstream stream;
        OStreamArchive     streamArchive(stream);
        JsonOutputArchive  archive(streamArchive);
        pWorld->SaveJson(archive);
        second = stream.str();
    }
    ASSERT_EQ(first, second);
}