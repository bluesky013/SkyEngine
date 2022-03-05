//
// Created by Zach Lee on 2022/3/5.
//

#include <gtest/gtest.h>
#include <core/math/Transform.h>
#include <string>

using namespace sky;

TEST(TransformTest, BasicTest)
{
    Transform trans = {};
    trans.translation = Vector3(1.f, 0.f, 0.f);
    trans.rotation = glm::angleAxis(30.f / 180.f * 3.1415926f, Vector3(0.f, 1.f, 0.f));

    Transform inverse = trans.GetInverse();
    Transform result = trans * inverse;

    ASSERT_FLOAT_EQ(result.rotation.w, 1.f);
    ASSERT_FLOAT_EQ(result.rotation.x, 0.f);
    ASSERT_FLOAT_EQ(result.rotation.y, 0.f);
    ASSERT_FLOAT_EQ(result.rotation.z, 0.f);
}

TEST(TransformTest, MatrixTest)
{
    Transform trans = {};
    trans.translation = Vector3(0.5f, 0.9f, 1.3f);
    trans.scale = Vector3(0.5f, 1.f, 2.f);
    trans.rotation = glm::angleAxis(30.f / 180.f * 3.1415926f, glm::normalize(Vector3(0.3f, 0.7f, 0.2f)));

    Matrix4 matrix = trans.ToMatrix();
    Transform result = Transform::FromMatrix(matrix);

    ASSERT_FLOAT_EQ(result.translation.x, trans.translation.x);
    ASSERT_FLOAT_EQ(result.translation.y, trans.translation.y);
    ASSERT_FLOAT_EQ(result.translation.z, trans.translation.z);

    ASSERT_FLOAT_EQ(result.scale.x, trans.scale.x);
    ASSERT_FLOAT_EQ(result.scale.y, trans.scale.y);
    ASSERT_FLOAT_EQ(result.scale.z, trans.scale.z);

    ASSERT_FLOAT_EQ(result.rotation.x, trans.rotation.x);
    ASSERT_FLOAT_EQ(result.rotation.y, trans.rotation.y);
    ASSERT_FLOAT_EQ(result.rotation.z, trans.rotation.z);
    ASSERT_FLOAT_EQ(result.rotation.w, trans.rotation.w);
}