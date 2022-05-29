//
// Created by Zach Lee on 2022/5/29.
//


#include <gtest/gtest.h>
#include <core/template/FixedChunkArray.h>

using namespace sky;

struct TestFCA {
    float a = 0.f;
    uint32_t b = 0;
};

TEST(FixedChunkArrayTest, BasicTest)
{
    const float fValue[4] = {1.f, 3.f, 5.f, 7.f};
    const uint32_t uValue[4] = {2, 4, 6, 8};

    FixedChunkArray<TestFCA, 2> array;

    array.Emplace({fValue[0], uValue[0]});
    array.Emplace({fValue[1], uValue[1]});
    array.Emplace({fValue[2], uValue[2]});
    array.Emplace({fValue[3], uValue[3]});

    uint32_t index = 0;
    array.ForEach([&](const TestFCA& value) {
        ASSERT_EQ(fValue[index], value.a);
        ASSERT_EQ(uValue[index], value.b);
        index++;
    });
}