//
// Created by blues on 2024/11/25.
//

#include <gtest/gtest.h>
#include <core/math/MathUtil.h>

using namespace sky;

TEST(MathTest, MathUtilTest)
{
    for (uint32_t i = 0; i < 32; ++i) {
        ASSERT_EQ(CeilLog2(static_cast<uint32_t>(pow(2, i))), i);
    }
}