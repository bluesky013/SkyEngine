//
// Created by Zach Lee on 2021/12/3.
//


#include <gtest/gtest.h>
#include <core/hash/Fnv1a.h>
#include <string>

static constexpr uint32_t hash32 = sky::Fnv1a32("SkyEngine");
static constexpr uint64_t hash64 = sky::Fnv1a64("SkyEngine");

TEST(UtilTest, TestFnv1aHash)
{
#ifdef MSVC
        ASSERT_EQ(hash32, std::hash<std::string>()("SkyEngine"));
#endif
}