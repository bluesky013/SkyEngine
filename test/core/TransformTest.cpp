//
// Created by Zach Lee on 2022/3/5.
//

#include <gtest/gtest.h>
#include <core/math/Transform.h>
#include <string>

using namespace sky;

TEST(TransformTest, BasicTest)
{
    Transform trans= {};
    trans.translation = Vector3(1.f, 0.f, 0.f);
    trans.rotation = glm::angleAxis(30.f / 180.f * 3.1415926f, Vector3(0.f, 1.f, 0.f));

    Transform inverse = trans.GetInverse();
    Transform result = trans * inverse;

    ASSERT_FLOAT_EQ(result.rotation.w, 1.f);
    ASSERT_FLOAT_EQ(result.rotation.x, 0.f);
    ASSERT_FLOAT_EQ(result.rotation.y, 0.f);
    ASSERT_FLOAT_EQ(result.rotation.z, 0.f);
}