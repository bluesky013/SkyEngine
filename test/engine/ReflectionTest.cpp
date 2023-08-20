//
// Created by Zach Lee on 2021/12/15.
//

#include <core/logger/Logger.h>
#include <engine/GlobalVariable.h>
#include <engine/SkyEngine.h>
#include <framework/world/TransformComponent.h>
#include <framework/serialization/SerializationUtil.h>
#include <gtest/gtest.h>

using namespace sky;

static const char *TAG = "EngineReflection";

TEST(EngineReflect, TestBasic)
{
    SkyEngine::Reflect();

    std::string output;
    {
        Any transform(Transform{});
        ASSERT_EQ(!!transform, true);
    }

    Component        *comp = new TransformComponent();
    const TypeInfoRT *info = comp->GetTypeInfo();

    auto node = GetTypeMember("local", info->typeId);
    ASSERT_NE(node, nullptr);

    auto local    = node->getterFn(comp);
    auto rotation = GetValue(local, node->info->typeId, "rotation");
    auto pos      = GetValue(local, node->info->typeId, "translation");
    auto scale    = GetValue(local, node->info->typeId, "scale");

    ASSERT_EQ(*static_cast<float *>(GetValue(pos, TypeInfo<Vector3>::Hash(), "x")), 0.f);
    ASSERT_EQ(*static_cast<float *>(GetValue(pos, TypeInfo<Vector3>::Hash(), "y")), 0.f);
    ASSERT_EQ(*static_cast<float *>(GetValue(pos, TypeInfo<Vector3>::Hash(), "z")), 0.f);

    ASSERT_EQ(*static_cast<float *>(GetValue(scale, TypeInfo<Vector3>::Hash(), "x")), 1.f);
    ASSERT_EQ(*static_cast<float *>(GetValue(scale, TypeInfo<Vector3>::Hash(), "y")), 1.f);
    ASSERT_EQ(*static_cast<float *>(GetValue(scale, TypeInfo<Vector3>::Hash(), "z")), 1.f);

    ASSERT_EQ(*static_cast<float *>(GetValue(rotation, TypeInfo<Quaternion>::Hash(), "x")), 0.f);
    ASSERT_EQ(*static_cast<float *>(GetValue(rotation, TypeInfo<Quaternion>::Hash(), "y")), 0.f);
    ASSERT_EQ(*static_cast<float *>(GetValue(rotation, TypeInfo<Quaternion>::Hash(), "z")), 0.f);
    ASSERT_EQ(*static_cast<float *>(GetValue(rotation, TypeInfo<Quaternion>::Hash(), "w")), 1.f);

    struct Test {
        int a;
        int b;
    };

    auto va            = &Test::a;
    Test p             = {1, 2};
    std::invoke(va, p) = 3;

    Test *q = &p;
    ASSERT_EQ(p.a, 3);
    std::invoke(va, q) = 4;
    ASSERT_EQ(p.a, 4);
}

TEST(GlobalVairableTest, ValueBasic)
{
    const std::string VALUE1("Test1 value");
    const int         VALUE2 = 1;
    const float       VALUE3 = 2;

    auto gv = GlobalVariable::Get();
    gv->Register("VALUE1", VALUE1);
    gv->Register("VALUE2", VALUE2);
    gv->Register("VALUE3", VALUE3);

    ASSERT_EQ(gv->Find<int>("VALUE1"), nullptr);
    ASSERT_EQ(*gv->Find<std::string>("VALUE1"), VALUE1);
    ASSERT_EQ(*gv->Find<int>("VALUE2"), VALUE2);
    ASSERT_EQ(*gv->Find<float>("VALUE3"), VALUE3);
}