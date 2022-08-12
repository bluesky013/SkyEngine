//
// Created by Zach Lee on 2021/12/15.
//

#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <framework/serialization/AnyRT.h>
#include <engine/SkyEngine.h>
#include <engine/world/TransformComponent.h>
#include <engine/GlobalVariable.h>

using namespace sky;

static const char* TAG = "EngineReflection";

TEST(EngineReflect, TestBasic)
{
    SkyEngine::Reflect();

    std::string output;
    {
        Any transform(Transform{});
        ASSERT_EQ(!!transform, true);
    }

    Component* comp = new TransformComponent();
    const TypeInfoRT* info = comp->GetTypeInfo();

    auto node = GetTypeMember("local", info);
    ASSERT_NE(node, nullptr);

    auto local = node->getterFn(comp, false);
    auto rotation = GetAny(local, "rotation");
    auto pos = GetAny(local, "translation");
    auto scale = GetAny(local, "scale");

    ASSERT_EQ(*GetAny(pos, "x").GetAs<float>(), 0.f);
    ASSERT_EQ(*GetAny(pos, "y").GetAs<float>(), 0.f);
    ASSERT_EQ(*GetAny(pos, "z").GetAs<float>(), 0.f);

    ASSERT_EQ(*GetAny(scale, "x").GetAs<float>(), 1.f);
    ASSERT_EQ(*GetAny(scale, "y").GetAs<float>(), 1.f);
    ASSERT_EQ(*GetAny(scale, "z").GetAs<float>(), 1.f);

    ASSERT_EQ(*GetAny(rotation, "x").GetAs<float>(), 0.f);
    ASSERT_EQ(*GetAny(rotation, "y").GetAs<float>(), 0.f);
    ASSERT_EQ(*GetAny(rotation, "z").GetAs<float>(), 0.f);
    ASSERT_EQ(*GetAny(rotation, "w").GetAs<float>(), 1.f);

    struct Test {
        int a;
        int b;
    };

    auto va = &Test::a;
    Test p = {1, 2};
    std::invoke(va, p) = 3;

    Test* q = &p;
    ASSERT_EQ(p.a, 3);
    std::invoke(va, q) = 4;
    ASSERT_EQ(p.a, 4);
}

TEST(GlobalVairableTest, ValueBasic)
{
    const std::string VALUE1("Test1 value");
    const int VALUE2 = 1;
    const float VALUE3 = 2;

    auto gv = GlobalVariable::Get();
    gv->Register("VALUE1", VALUE1);
    gv->Register("VALUE2", VALUE2);
    gv->Register("VALUE3", VALUE3);

    ASSERT_EQ(gv->Find<int>("VALUE1"), nullptr);
    ASSERT_EQ(*gv->Find<std::string>("VALUE1"), VALUE1);
    ASSERT_EQ(*gv->Find<int>("VALUE2"), VALUE2);
    ASSERT_EQ(*gv->Find<float>("VALUE3"), VALUE3);
}