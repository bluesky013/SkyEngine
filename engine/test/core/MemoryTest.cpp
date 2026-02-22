//
// Created by Zach Lee on 2022/11/20.
//
#include <core/memory/TLSFAllocator.h>
#include <gtest/gtest.h>

using namespace sky;

TEST(MemoryTest, BitScanTest)
{
    {
        uint64_t value = (1ULL << 33) | (1ULL << 34);
        {
            uint32_t val = BitScan(value);
            ASSERT_EQ(val, 33);
        }
        {
            uint32_t val = BitScanReverse(value);
            ASSERT_EQ(val, 34);
        }
    }
    {
        uint32_t value = (1 << 10) | (1 << 11);
        {
            uint32_t val = BitScan(value);
            ASSERT_EQ(val, 10);
        }
        {
            uint32_t val = BitScanReverse(value);
            ASSERT_EQ(val, 11);
        }
    }

}

TEST(MemoryTest, TLSFLevelMappingTest)
{
    TLSFPool pool;
    pool.Init();

    {
        auto [fl, sl] = pool.LevelMapping(16);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 1);
    }
    {
        auto [fl, sl] = pool.LevelMapping(17);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 1);
    }
    {
        auto [fl, sl] = pool.LevelMapping(32);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 2);
    }
    {
        auto [fl, sl] = pool.LevelMapping(240);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 15);
    }
    {
        auto [fl, sl] = pool.LevelMapping(255);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 15);
    }
    {
        auto [fl, sl] = pool.LevelMapping(256);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 0);
    }
    {
        auto [fl, sl] = pool.LevelMapping(257);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 0);
    }
    {
        auto [fl, sl] = pool.LevelMapping(256 + 15);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 0);
    }
    {
        auto [fl, sl] = pool.LevelMapping(256 + 16);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 1);
    }
    {
        auto [fl, sl] = pool.LevelMapping(256 + 17);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 1);
    }
    {
        auto [fl, sl] = pool.LevelMapping(256 + 32);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 2);
    }
    {
        auto [fl, sl] = pool.LevelMapping(256 + 240);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 15);
    }
    {
        auto [fl, sl] = pool.LevelMapping(256 + 241);
        ASSERT_EQ(fl, 1);
        ASSERT_EQ(sl, 15);
    }
    {
        auto [fl, sl] = pool.LevelMapping(512);
        ASSERT_EQ(fl, 2);
        ASSERT_EQ(sl, 0);
    }
}

TEST(MemoryTest, TLSFAllocateTestNormal)
{
    TLSFPool pool;
    pool.Init();

    auto *block1 = pool.Allocate(65536, 256);
    {
        ASSERT_EQ(block1->offset, 0);
        ASSERT_EQ(block1->size, 65536);
    }

    auto *block2 = pool.Allocate(65536, 256);
    {
        ASSERT_EQ(block2->offset, 65536);
        ASSERT_EQ(block2->size, 65536);
    }
    pool.Free(block1);
    pool.Free(block2);

    auto *block3 = pool.Allocate(65536 * 2, 256);
    {
        ASSERT_EQ(block3->offset, 0);
        ASSERT_EQ(block3->size, 65536 *2);
    }
}
