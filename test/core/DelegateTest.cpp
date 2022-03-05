//
// Created by Zach Lee on 2022/3/6.
//

#include <gtest/gtest.h>
#include <core/event/Delegate.h>
#include <string>

using namespace sky;

void Func(int& v)
{
    v = 1;
}

TEST(DelegateTest, SimpleTest)
{
    int value = 0;
    Delegate<void(int&)> delegate;
    delegate.Connect<&Func>();
    delegate(value);
    delegate.Reset();

    ASSERT_EQ(value, 1);
}