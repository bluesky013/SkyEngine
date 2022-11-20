//
// Created by Zach Lee on 2022/11/20.
//
#include <core/memory/TLSFAllocator.h>
#include <gtest/gtest.h>

using namespace sky;

TEST(MemoryTest, TLSFAllocatorTest)
{
    TLSFPool pool;
    pool.Init(0);
}