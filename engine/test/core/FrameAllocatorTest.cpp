//
// Created by Zach Lee on 2026/09/02.
//

#include <core/memory/FrameAllocator.h>
#include <gtest/gtest.h>

using namespace sky;

// ============================================================
// FrameAllocator tests
// ============================================================
TEST(FrameAllocatorTest, BasicAllocateTest)
{
    FrameAllocator alloc(256);

    auto *p1 = alloc.Allocate(64);
    ASSERT_NE(p1, nullptr);

    auto *p2 = alloc.Allocate(64);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p1, p2);
}

TEST(FrameAllocatorTest, StatisticsTest)
{
    FrameAllocator alloc(256);

    ASSERT_EQ(alloc.GetCurrentBytes(), 0u);
    ASSERT_EQ(alloc.GetPeakBytes(), 0u);
    ASSERT_EQ(alloc.GetAllocationCount(), 0u);

    alloc.Allocate(100);
    ASSERT_EQ(alloc.GetCurrentBytes(), 100u);
    ASSERT_EQ(alloc.GetPeakBytes(), 100u);
    ASSERT_EQ(alloc.GetAllocationCount(), 1u);

    alloc.Allocate(50);
    ASSERT_EQ(alloc.GetCurrentBytes(), 150u);
    ASSERT_EQ(alloc.GetPeakBytes(), 150u);
    ASSERT_EQ(alloc.GetAllocationCount(), 2u);

    alloc.Allocate(10);
    ASSERT_EQ(alloc.GetCurrentBytes(), 160u);
    ASSERT_EQ(alloc.GetPeakBytes(), 160u);
    ASSERT_EQ(alloc.GetAllocationCount(), 3u);
}

TEST(FrameAllocatorTest, RewindUpdatesStatisticsTest)
{
    FrameAllocator alloc(256);

    auto mark0 = alloc.GetMark();
    alloc.Allocate(100);
    ASSERT_EQ(alloc.GetCurrentBytes(), 100u);
    ASSERT_EQ(alloc.GetPeakBytes(), 100u);

    auto mark1 = alloc.GetMark();
    alloc.Allocate(50);
    ASSERT_EQ(alloc.GetCurrentBytes(), 150u);
    ASSERT_EQ(alloc.GetPeakBytes(), 150u);

    alloc.Rewind(mark1);
    ASSERT_EQ(alloc.GetCurrentBytes(), 100u);
    ASSERT_EQ(alloc.GetPeakBytes(), 150u); // peak remains

    alloc.Rewind(mark0);
    ASSERT_EQ(alloc.GetCurrentBytes(), 0u);
    ASSERT_EQ(alloc.GetPeakBytes(), 150u); // peak remains
}

TEST(FrameAllocatorTest, ResetUpdatesStatisticsTest)
{
    FrameAllocator alloc(256);

    alloc.Allocate(100);
    alloc.Allocate(100);
    ASSERT_EQ(alloc.GetCurrentBytes(), 200u);
    ASSERT_EQ(alloc.GetPeakBytes(), 200u);
    ASSERT_EQ(alloc.GetAllocationCount(), 2u);

    alloc.Reset();
    ASSERT_EQ(alloc.GetCurrentBytes(), 0u);
    ASSERT_EQ(alloc.GetPeakBytes(), 0u);
    ASSERT_EQ(alloc.GetAllocationCount(), 0u);
}

TEST(FrameAllocatorTest, AllocateArrayTest)
{
    FrameAllocator alloc(256);

    auto *arr = alloc.AllocateArray<int>(10);
    ASSERT_NE(arr, nullptr);

    for (int i = 0; i < 10; ++i) {
        arr[i] = i * 10;
    }
    ASSERT_EQ(arr[5], 50);
}

TEST(FrameAllocatorTest, ConstructTest)
{
    FrameAllocator alloc(256);

    struct TestStruct {
        int a;
        float b;
        TestStruct(int x, float y) : a(x), b(y) {}
    };

    auto *obj = alloc.Construct<TestStruct>(42, 3.14f);
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->a, 42);
    ASSERT_FLOAT_EQ(obj->b, 3.14f);
}

TEST(FrameAllocatorTest, CrossBlockTest)
{
    FrameAllocator alloc(64);

    alloc.Allocate(60);
    auto mark = alloc.GetMark();

    alloc.Allocate(60); // triggers block 1
    ASSERT_EQ(alloc.GetCurrentBytes(), 120u);

    alloc.Rewind(mark);
    ASSERT_EQ(alloc.GetCurrentBytes(), 60u);
}

TEST(FrameAllocatorTest, WithTransientVectorTest)
{
    FrameAllocator alloc(4096);

    auto vec = MakeTransientVector<int>(alloc.Arena());
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    ASSERT_EQ(vec.size(), 3u);
    ASSERT_EQ(vec[0], 1);
    ASSERT_EQ(vec[2], 3);
    // TransientStdAllocator calls mArena.Allocate directly, so FrameAllocator stats
    // only track explicit Allocate() calls, not container growth
    ASSERT_EQ(alloc.GetCurrentBytes(), 0u);
    ASSERT_GT(alloc.Arena().GetCurrentUsedSize(), 0u);
}

TEST(FrameAllocatorTest, RewindAfterVectorTest)
{
    FrameAllocator alloc(4096);

    auto mark = alloc.GetMark();

    {
        auto vec = MakeTransientVector<int>(alloc.Arena());
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }
        // TransientStdAllocator calls mArena.Allocate directly, not FrameAllocator::Allocate
        ASSERT_GT(alloc.Arena().GetCurrentUsedSize(), 0u);
    }

    alloc.Rewind(mark);
    ASSERT_EQ(alloc.GetCurrentBytes(), 0u);
    ASSERT_EQ(alloc.Arena().GetCurrentUsedSize(), 0u);
}

TEST(FrameAllocatorTest, MultipleMarksTest)
{
    FrameAllocator alloc(256);

    auto m0 = alloc.GetMark();
    alloc.Allocate(10);
    auto m1 = alloc.GetMark();
    alloc.Allocate(20);
    auto m2 = alloc.GetMark();
    alloc.Allocate(30);

    ASSERT_EQ(alloc.GetCurrentBytes(), 60u);

    alloc.Rewind(m2);
    ASSERT_EQ(alloc.GetCurrentBytes(), 30u);

    alloc.Rewind(m1);
    ASSERT_EQ(alloc.GetCurrentBytes(), 10u);

    alloc.Rewind(m0);
    ASSERT_EQ(alloc.GetCurrentBytes(), 0u);
}
