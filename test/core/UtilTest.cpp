//
// Created by Zach Lee on 2021/12/3.
//


#include <gtest/gtest.h>
#include <core/hash/Fnv1a.h>
#include <core/util/Uuid.h>
#include <core/util/Memory.h>
#include <string>

static constexpr uint32_t hash32 = sky::Fnv1a32("SkyEngine");
static constexpr uint64_t hash64 = sky::Fnv1a64("SkyEngine");

using namespace sky;

struct TestUuid {
    static constexpr Uuid ID = Uuid::CreateFromString("12345678-0123-4567-8901-234567890123");
};

TEST(UtilTest, TestFnv1aHash)
{
#ifdef MSVC
        ASSERT_EQ(hash32, std::hash<std::string>()("SkyEngine"));
#endif
}

TEST(MathTest, UuIdTest)
{
    {
        Uuid id = Uuid::Create();
        std::cout << id.ToString() << std::endl;
    }

    {
        Uuid id;
        std::cout << id.ToString() << std::endl;
    }


    std::string str = TestUuid::ID.ToString();
    ASSERT_EQ(str, std::string("12345678-0123-4567-8901-234567890123"));
}

TEST(MemoryTest, AlignTest)
{
    ASSERT_EQ(Align(3, 4), 4);
    ASSERT_EQ(Align(5, 4), 8);
    ASSERT_EQ(Align(3, 8), 8);
}