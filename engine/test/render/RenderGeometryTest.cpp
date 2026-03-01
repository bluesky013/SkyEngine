//
// Created by blues on 2025/5/18.
//

#include <gtest/gtest.h>
#include <render/geometry/GeometryFactory.h>

using namespace sky;

TEST(RenderGeometryTest, GeometryFacotryTest)
{
    auto cube = GeometryFactory{}.CreateGeometry(BuiltinGeometryType::CUBE);

    printf("test\n");

}