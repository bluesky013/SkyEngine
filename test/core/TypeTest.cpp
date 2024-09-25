//
// Created by blues on 2024/4/22.
//

#include <core/logger/Logger.h>
#include <core/type/Type.h>
#include <gtest/gtest.h>
#include <iostream>

using namespace sky;

struct FuncTypeTest {
    void f1() const {}
    void f2() {}
};

TEST(TypeTest, FuncTypeTest01)
{
    ASSERT_EQ(FuncTraits<decltype(&FuncTypeTest::f1)>::CONST, true);
    ASSERT_EQ(FuncTraits<decltype(&FuncTypeTest::f2)>::CONST, false);
}