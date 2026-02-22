//
// Created by blues on 2026/2/20.
//

#include <core/memory/LinearStorage.h>
#include <core/memory/TransientAllocator.h>
#include <gtest/gtest.h>

using namespace sky;

// ============================================================
// LinearStorage tests
// ============================================================
TEST(LinearStorageTest, BasicAllocateTest)
{
    LinearStorage storage(256);

    auto *p1 = storage.Allocate(64);
    ASSERT_NE(p1, nullptr);

    auto *p2 = storage.Allocate(64);
    ASSERT_NE(p2, nullptr);
    ASSERT_EQ(p2, p1 + 64);
}

TEST(LinearStorageTest, ExpandsToNewBlockTest)
{
    LinearStorage storage(64);

    auto *p1 = storage.Allocate(60);
    ASSERT_NE(p1, nullptr);
    ASSERT_EQ(storage.GetBlockCount(), 1u);

    // Only 4 bytes left, this won't fit
    auto *p2 = storage.Allocate(32);
    ASSERT_NE(p2, nullptr);
    ASSERT_EQ(storage.GetBlockCount(), 2u);
}

TEST(LinearStorageTest, ResetRewindsAllBlocksTest)
{
    LinearStorage storage(64);

    auto *first = storage.Allocate(60);
    storage.Allocate(60); // triggers second block
    ASSERT_EQ(storage.GetBlockCount(), 2u);

    storage.Reset();

    // After reset, should reuse first block, same pointer returned
    auto *p = storage.Allocate(60);
    ASSERT_EQ(p, first);
    // Block count stays at 2 (blocks are retained, not freed)
    ASSERT_EQ(storage.GetBlockCount(), 2u);
}

TEST(LinearStorageTest, ResetReusesBlocksTest)
{
    LinearStorage storage(128);

    for (int i = 0; i < 10; ++i) {
        storage.Allocate(100);
    }
    size_t blockCount = storage.GetBlockCount();
    ASSERT_GT(blockCount, 1u);

    storage.Reset();

    // Allocate same amount again — should not create new blocks
    for (int i = 0; i < 10; ++i) {
        auto *p = storage.Allocate(100);
        ASSERT_NE(p, nullptr);
    }
    ASSERT_EQ(storage.GetBlockCount(), blockCount);
}

TEST(LinearStorageTest, GetTotalCapacityTest)
{
    LinearStorage storage(256);
    ASSERT_EQ(storage.GetTotalCapacity(), 256u);

    storage.Allocate(256);
    storage.Allocate(1); // triggers new block
    ASSERT_EQ(storage.GetTotalCapacity(), 512u);
}

TEST(LinearStorageTest, GetCurrentUsedSizeTest)
{
    LinearStorage storage(128);
    ASSERT_EQ(storage.GetCurrentUsedSize(), 0u);

    storage.Allocate(50);
    ASSERT_EQ(storage.GetCurrentUsedSize(), 50u);

    storage.Allocate(30);
    ASSERT_EQ(storage.GetCurrentUsedSize(), 80u);

    // Trigger second block
    storage.Allocate(100);
    // First block full (128 used as base), second block has 100
    ASSERT_EQ(storage.GetCurrentUsedSize(), 128u + 100u);

    storage.Reset();
    ASSERT_EQ(storage.GetCurrentUsedSize(), 0u);
}

TEST(LinearStorageTest, ShrinkReleasesUnusedBlocksTest)
{
    LinearStorage storage(64);

    // Create 5 blocks
    for (int i = 0; i < 5; ++i) {
        storage.Allocate(60);
    }
    ASSERT_EQ(storage.GetBlockCount(), 5u);

    storage.Reset();

    // Use only 2 blocks
    storage.Allocate(60);
    storage.Allocate(60);
    ASSERT_EQ(storage.GetBlockCount(), 5u); // still 5 before shrink

    storage.Shrink();
    ASSERT_EQ(storage.GetBlockCount(), 2u); // trimmed to 2
}

TEST(LinearStorageTest, ShrinkOnSingleBlockTest)
{
    LinearStorage storage(256);
    storage.Allocate(100);

    storage.Shrink(); // only 1 block, nothing to remove
    ASSERT_EQ(storage.GetBlockCount(), 1u);
}

TEST(LinearStorageTest, AlignmentCrossBlockBoundaryTest)
{
    LinearStorage storage(64);

    // Fill first block almost completely
    storage.Allocate(63);
    // Only 1 byte left — aligned allocation for 16 bytes with alignment 16
    // must go to next block
    auto *p = storage.Allocate(16, 16);
    ASSERT_NE(p, nullptr);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u);
    ASSERT_EQ(storage.GetBlockCount(), 2u);
}

TEST(LinearStorageTest, ExactFitTest)
{
    LinearStorage storage(64);

    auto *p = storage.Allocate(64);
    ASSERT_NE(p, nullptr);
    ASSERT_EQ(storage.GetBlockCount(), 1u);

    // Block is exactly full, next allocation goes to new block
    auto *p2 = storage.Allocate(1);
    ASSERT_NE(p2, nullptr);
    ASSERT_EQ(storage.GetBlockCount(), 2u);
}

// ============================================================
// TransientAllocator tests
// ============================================================
TEST(TransientAllocatorTest, BasicAllocateAndResetTest)
{
    TransientAllocator alloc(256);

    auto *p1 = alloc.Allocate(64);
    ASSERT_NE(p1, nullptr);

    auto *p2 = alloc.Allocate(64);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p1, p2);

    alloc.Reset();

    // After reset, the same memory region should be returned
    auto *p3 = alloc.Allocate(64);
    ASSERT_EQ(p3, p1);
}

TEST(TransientAllocatorTest, ExternalStorageTest)
{
    LinearStorage storage(128);
    TransientAllocator alloc(storage);

    auto *p = alloc.Allocate(64);
    ASSERT_NE(p, nullptr);

    ASSERT_EQ(&alloc.GetStorage(), &storage);
}

TEST(TransientAllocatorTest, DeallocateIsNoopTest)
{
    TransientAllocator alloc(256);

    auto *p = alloc.Allocate(64);
    ASSERT_NE(p, nullptr);
    // Should not crash or do anything
    alloc.Deallocate(p, 64);
}

TEST(TransientAllocatorTest, ShrinkTest)
{
    TransientAllocator alloc(64);

    for (int i = 0; i < 10; ++i) {
        alloc.Allocate(60);
    }
    ASSERT_GT(alloc.GetStorage().GetBlockCount(), 1u);

    alloc.Reset();
    alloc.Allocate(60); // use only 1 block
    alloc.Shrink();
    ASSERT_EQ(alloc.GetStorage().GetBlockCount(), 1u);
}

TEST(TransientAllocatorTest, GetCurrentUsedSizeTest)
{
    TransientAllocator alloc(256);

    ASSERT_EQ(alloc.GetCurrentUsedSize(), 0u);

    alloc.Allocate(100, 1);
    ASSERT_EQ(alloc.GetCurrentUsedSize(), 100u);

    alloc.Reset();
    ASSERT_EQ(alloc.GetCurrentUsedSize(), 0u);
}

TEST(TransientStdAllocatorTest, StringBasicTest)
{
    TransientAllocator alloc(4096);
    TransientString str{TransientStdAllocator<char>{alloc}};

    str = "Hello, Transient World!";
    ASSERT_EQ(str, "Hello, Transient World!");
    ASSERT_EQ(str.size(), 23u);
}

TEST(TransientStdAllocatorTest, StringLongTest)
{
    TransientAllocator alloc(4096);
    TransientString str{TransientStdAllocator<char>{alloc}};

    // SSO threshold is typically 15-22 chars, exceed it
    std::string longStr(200, 'x');
    str = longStr.c_str();
    ASSERT_EQ(str.size(), 200u);
    ASSERT_EQ(str, longStr.c_str());
}

TEST(TransientStdAllocatorTest, ListBasicTest)
{
    TransientAllocator alloc(4096);
    TransientList<int> lst{TransientStdAllocator<int>{alloc}};

    lst.push_back(10);
    lst.push_back(20);
    lst.push_front(5);

    ASSERT_EQ(lst.size(), 3u);
    auto it = lst.begin();
    ASSERT_EQ(*it++, 5);
    ASSERT_EQ(*it++, 10);
    ASSERT_EQ(*it++, 20);
}

TEST(TransientStdAllocatorTest, HashMapBasicTest)
{
    TransientAllocator alloc(4096);
    TransientHashMap<int, int> map(
        0, std::hash<int>{}, std::equal_to<int>{},
        TransientStdAllocator<std::pair<const int, int>>(alloc));

    map[1] = 100;
    map[2] = 200;
    map[3] = 300;

    ASSERT_EQ(map.size(), 3u);
    ASSERT_EQ(map[1], 100);
    ASSERT_EQ(map[2], 200);
    ASSERT_EQ(map[3], 300);
}

TEST(TransientStdAllocatorTest, AllocatorEqualityTest)
{
    TransientAllocator alloc1(256);
    TransientAllocator alloc2(256);

    TransientStdAllocator<int> a1(alloc1);
    TransientStdAllocator<int> a2(alloc1);
    TransientStdAllocator<int> a3(alloc2);

    ASSERT_TRUE(a1 == a2);
    ASSERT_TRUE(a1 != a3);
}

TEST(TransientStdAllocatorTest, RebindAcrossTypesTest)
{
    TransientAllocator alloc(4096);
    TransientStdAllocator<int> intAlloc(alloc);
    TransientStdAllocator<double> dblAlloc(intAlloc); // rebind constructor

    ASSERT_EQ(intAlloc.GetAllocator(), dblAlloc.GetAllocator());
}

TEST(TransientStdAllocatorTest, MultiFrameWithMixedContainersTest)
{
    TransientAllocator alloc(2048);

    for (int frame = 0; frame < 3; ++frame) {
        {
            TransientString str{TransientStdAllocator<char>{alloc}};
            TransientList<float> lst{TransientStdAllocator<float>{alloc}};

            for (int i = 0; i < 20; ++i) {
                lst.push_back(static_cast<float>(i));
            }
            str = "frame data for rendering";

            ASSERT_EQ(lst.size(), 20u);
            ASSERT_FALSE(str.empty());
        }
        alloc.Reset();
    }
}

TEST(TransientStdAllocatorTest, MakeTransientStringTest)
{
    TransientAllocator alloc(4096);
    auto str = MakeTransientString(alloc);

    str = "factory helper works";
    ASSERT_EQ(str, "factory helper works");
}

TEST(TransientStdAllocatorTest, MakeTransientListTest)
{
    TransientAllocator alloc(4096);
    auto lst = MakeTransientList<double>(alloc);

    lst.push_back(1.5);
    lst.push_back(2.5);
    lst.push_front(0.5);

    ASSERT_EQ(lst.size(), 3u);
    ASSERT_DOUBLE_EQ(lst.front(), 0.5);
    ASSERT_DOUBLE_EQ(lst.back(), 2.5);
}

TEST(TransientStdAllocatorTest, MakeTransientHashMapTest)
{
    TransientAllocator alloc(4096);
    auto map = MakeTransientHashMap<int, int>(alloc);

    map[1] = 10;
    map[2] = 20;

    ASSERT_EQ(map.size(), 2u);
    ASSERT_EQ(map[1], 10);
    ASSERT_EQ(map[2], 20);
}
