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

TEST(MemoryTest, TLSFAllocatorTest)
{
    TLSFPool pool;
    pool.Init(0);

    {
        auto [fl, sl] = TLSFPool::LevelMappingRoundUp(4);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 0);
    }
    {
        auto [fl, sl] = TLSFPool::LevelMappingRoundUp(16);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 0);
    }
    {
        auto [fl, sl] = TLSFPool::LevelMappingRoundUp(32);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 1);
    }
    {
        auto [fl, sl] = TLSFPool::LevelMappingRoundUp(255);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 15);
    }
    {
        auto [fl, sl] = TLSFPool::LevelMappingRoundUp(256);
        ASSERT_EQ(fl, 0);
        ASSERT_EQ(sl, 15);
    }
    {
        auto [fl, sl] = TLSFPool::LevelMappingRoundUp(257);
        ASSERT_EQ(fl, 1);
//        ASSERT_EQ(sl, 0);
    }
    {
        auto [fl, sl] = TLSFPool::LevelMappingRoundUp(512);
        ASSERT_EQ(fl, 1);
//        ASSERT_EQ(sl, 0);
    }
}
