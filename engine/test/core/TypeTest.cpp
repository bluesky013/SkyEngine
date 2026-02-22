//
// Created by blues on 2024/4/22.
//

#include <core/logger/Logger.h>
#include <core/type/Type.h>
#include <core/type/Container.h>
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

TEST(TypeTest, ContainerTest01)
{
    ASSERT_EQ(CheckViewType<int>::value, false);
    ASSERT_EQ(CheckViewType<std::string>::value, false);
    ASSERT_EQ(CheckViewType<std::string_view>::value, true);

    ASSERT_EQ(ContainerTraits<bool>::IS_SEQUENCE, false);
    ASSERT_EQ(ContainerTraits<std::vector<int>>::IS_SEQUENCE, true);
    ASSERT_EQ(ContainerTraits<std::string>::IS_SEQUENCE, true);
    ASSERT_EQ(ContainerTraits<std::string_view>::IS_SEQUENCE, false);
}