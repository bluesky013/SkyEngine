//
// Created by Zach Lee on 2021/12/15.
//

#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <engine/SkyEngine.h>
#include <engine/world/TransformComponent.h>

using namespace sky;

static const char* TAG = "EngineReflection";

TEST(EngineReflect, TestBasic)
{
    SkyEngine::Reflect();

    std::string output;
    {
        Any transform(Transform{});
        ASSERT_EQ(!!transform, true);

        SerializationWriteString(transform, output);
        LOG_I(TAG, "%s\n", output.data());
    }

    Component* comp = new TransformComponent();
    const TypeInfoRT* info = comp->GetTypeInfo();

    auto node = GetTypeMember("local", info);
    ASSERT_NE(node, nullptr);

    auto local = node->getterFn(comp, false);
    auto rotation = local.Get("rotation");
    auto pos = local.Get("pos");
    auto scale = local.Get("scale");

    ASSERT_EQ(*pos.Get("x").GetAs<float>(), 0.f);
    ASSERT_EQ(*pos.Get("y").GetAs<float>(), 0.f);
    ASSERT_EQ(*pos.Get("z").GetAs<float>(), 0.f);

    ASSERT_EQ(*scale.Get("x").GetAs<float>(), 1.f);
    ASSERT_EQ(*scale.Get("y").GetAs<float>(), 1.f);
    ASSERT_EQ(*scale.Get("z").GetAs<float>(), 1.f);

    ASSERT_EQ(*rotation.Get("x").GetAs<float>(), 0.f);
    ASSERT_EQ(*rotation.Get("y").GetAs<float>(), 0.f);
    ASSERT_EQ(*rotation.Get("z").GetAs<float>(), 0.f);
    ASSERT_EQ(*rotation.Get("w").GetAs<float>(), 1.f);

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