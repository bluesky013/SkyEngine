//
// Created by Zach Lee on 2022/5/29.
//

#include <core/template/FixedChunkArray.h>
#include <core/template/ObjectPool.h>
#include <gtest/gtest.h>

using namespace sky;

struct TestFCA {
    float    a = 0.f;
    uint32_t b = 0;
};

TEST(TemplateTest, FixedChunkArrayTest)
{
    const float    fValue[4] = {1.f, 3.f, 5.f, 7.f};
    const uint32_t uValue[4] = {2, 4, 6, 8};

    FixedChunkArray<TestFCA, 2> array;

    array.Emplace({fValue[0], uValue[0]});
    array.Emplace({fValue[1], uValue[1]});
    array.Emplace({fValue[2], uValue[2]});
    array.Emplace({fValue[3], uValue[3]});

    uint32_t index = 0;
    array.ForEach([&](const TestFCA &value) {
        ASSERT_EQ(fValue[index], value.a);
        ASSERT_EQ(uValue[index], value.b);
        index++;
    });
}

struct TestPoolData {
    int a = 1;
    float b = 2.f;
};

TEST(TemplateTest, ObjectPoolTest)
{
    ObjectPool<TestPoolData> data(2);

    TestPoolData *p1 = data.Allocate();
    ASSERT_EQ(p1->a, 1);
    ASSERT_EQ(p1->b, 2.f);

    TestPoolData *p2 = data.Allocate(TestPoolData{2, 3.f});
    ASSERT_EQ(p2->a, 2);
    ASSERT_EQ(p2->b, 3.f);

    ASSERT_EQ(p2, &p1[1]);

    data.Free(p2);
    TestPoolData *p3 = data.Allocate();
    ASSERT_EQ(p2, p3);
    ASSERT_EQ(p3->a, 1);
    ASSERT_EQ(p3->b, 2.f);

    TestPoolData *p4 = data.Allocate(TestPoolData{5, 6.f});
    TestPoolData *p5 = data.Allocate(TestPoolData{7, 8.f});

    ASSERT_EQ(p4->a, 5);
    ASSERT_EQ(p4->b, 6.f);
    ASSERT_EQ(p5->a, 7);
    ASSERT_EQ(p5->b, 8.f);
}
