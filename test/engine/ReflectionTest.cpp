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
    Any any = comp->GetAsRef();
    TransformComponent* trans = *any.GetAs<TransformComponent*>();
    ASSERT_NE(trans, nullptr);
    ASSERT_EQ(trans->local.scale.x, 1.f);
    ASSERT_EQ(trans->local.scale.y, 1.f);
    ASSERT_EQ(trans->local.scale.z, 1.f);

    ASSERT_EQ(trans->local.pos.x, 0.f);
    ASSERT_EQ(trans->local.pos.y, 0.f);
    ASSERT_EQ(trans->local.pos.z, 0.f);

    ASSERT_EQ(trans->local.rotation.x, 0.f);
    ASSERT_EQ(trans->local.rotation.y, 0.f);
    ASSERT_EQ(trans->local.rotation.z, 0.f);
    ASSERT_EQ(trans->local.rotation.w, 1.f);

}