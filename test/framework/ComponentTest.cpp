//
// Created by blues on 2024/5/14.
//

#include <framework/world/World.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <framework/serialization/SerializationUtil.h>
#include <fstream>
#include <gtest/gtest.h>

using namespace sky;

struct TestComponentData {
    int a;
    float b;
};

class TestComponent : public ComponentAdaptor<TestComponentData> {
public:
    TestComponent() = default;
    ~TestComponent() override = default;

    COMPONENT_RUNTIME_INFO(TestComponent)

    static void Reflect(SerializationContext *context)
    {
        context->Register<TestComponentData>("TestComponentData")
                .Member<&TestComponentData::a>("a")
                .Member<&TestComponentData::b>("b");


        REGISTER_BEGIN(TestComponent, context)
            REGISTER_MEMBER(a, SetA, GetA)
            REGISTER_MEMBER(b, SetB, GetB);
    }

    void SetA(int a) { data.a = a; }
    int GetA() const { return data.a; }

    void SetB(float b) { data.b = b; }
    float GetB() const { return data.b; }
};

class ComponentTest : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        auto *context = SerializationContext::Get();
        TestComponent::Reflect(context);
        TransformComponent::Reflect(context);
    }

    static void TearDownTestSuite()
    {
    }
};

TEST_F(ComponentTest, ActorTest)
{
    std::unique_ptr<World> pWorld(World::CreateWorld());
    auto &world = *pWorld;

    auto id = Uuid::CreateWithSeed(0);
    ActorPtr actor = world.CreateActor(id);

    {
        auto* comp = actor->AddComponent<TestComponent>();
        comp->SetA(1);
        comp->SetB(2.f);

        ASSERT_EQ(comp->GetA(), 1);
        ASSERT_EQ(comp->GetB(), 2.f);
    }

    {
        auto* comp = actor->GetComponent<TestComponent>();
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
        std::ofstream stream("ActorTest.json");
        OStreamArchive streamArchive(stream);
        JsonOutputArchive archive(streamArchive);

        world.SaveJson(archive);
    }

    {
        actor->RemoveComponent<TestComponent>();
        auto* comp = actor->GetComponent<TestComponent>();
        ASSERT_EQ(comp, nullptr);

        actor->RemoveComponent(Uuid{});

        world.DetachFromWorld(actor);
    }

    {
        std::ifstream stream("ActorTest.json");
        IStreamArchive streamArchive(stream);
        JsonInputArchive archive(streamArchive);

        world.LoadJson(archive);

        auto tActor = world.GetActorByUuid(id);
        ASSERT_NE(tActor, nullptr);

        auto* comp = tActor->GetComponent<TestComponent>();
        ASSERT_NE(tActor, nullptr);
        ASSERT_EQ(comp->GetA(), 1);
        ASSERT_EQ(comp->GetB(), 2.f);
    }
}

//TEST_F(ComponentTest, ActorHierarchy)
//{
//    {
//        std::unique_ptr<World> world(World::CreateWorld());
//
//        auto *actor1 = world->CreateActor();
//        auto *actor2 = world->CreateActor();
//        auto *actor3 = world->CreateActor();
//
//        actor3->SetParent(actor1);
//        ASSERT_EQ(actor1->GetChildren()[0], actor3);
//        ASSERT_EQ(actor2->GetChildren().size(), 0);
//        ASSERT_EQ(actor3->GetParent(), actor1);
//
//        actor3->SetParent(actor2);
//        ASSERT_EQ(actor1->GetChildren().size(), 0);
//        ASSERT_EQ(actor2->GetChildren()[0], actor3);
//        ASSERT_EQ(actor3->GetParent(), actor2);
//    }
//
//    {
//        std::unique_ptr<World> world(World::CreateWorld());
//
//        auto *actor1 = world->CreateActor();
//        auto *actor2 = world->CreateActor();
//        auto *actor3 = world->CreateActor();
//
//        actor2->SetParent(actor1);
//        actor3->SetParent(actor2);
//
//        world->DestroyActor(actor1);
//        ASSERT_EQ(actor2->GetChildren()[0], actor3);
//        ASSERT_EQ(actor2->GetParent(), world->GetRoot());
//        ASSERT_EQ(actor3->GetParent(), actor2);
//    }
//
//}

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