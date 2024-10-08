//
// Created by Zach Lee on 2021/12/3.
//

#include <core/hash/Fnv1a.h>
#include <core/util/Memory.h>
#include <core/util/Uuid.h>
#include <core/util/ArrayBitFlag.h>
#include <gtest/gtest.h>
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

TEST(UtilTest, UuIdTest)
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

TEST(UtilTest, AlignTest)
{
    ASSERT_EQ(Align(3, 4), 4);
    ASSERT_EQ(Align(5, 4), 8);
    ASSERT_EQ(Align(3, 8), 8);
}

enum ArrayBitTestE : uint32_t {
    VAL1 = 1,
    VAL4 = 4,
    VAL32 = 32,
    VAL33 = 33,
    MAX
};
TEST(UtilTest, ArrayBitTest)
{
    ArrayBit<ArrayBitTestE, ArrayBitTestE::MAX> bit;
    bit.SetBit(ArrayBitTestE::VAL4);
    bit.SetBit(ArrayBitTestE::VAL33);
    ASSERT_EQ(bit.CheckBit(ArrayBitTestE::VAL1), false);
    ASSERT_EQ(bit.CheckBit(ArrayBitTestE::VAL4), true);
    ASSERT_EQ(bit.CheckBit(ArrayBitTestE::VAL32), false);
    ASSERT_EQ(bit.CheckBit(ArrayBitTestE::VAL33), true);

    bit.ResetBit(ArrayBitTestE::VAL4);
    ASSERT_EQ(bit.CheckBit(ArrayBitTestE::VAL4), false);

    bit.ResetBit(ArrayBitTestE::VAL33);
    ASSERT_EQ(bit.CheckBit(ArrayBitTestE::VAL33), false);
}

struct MemoryBuf {
    std::vector<uint8_t> data;
};

TEST(UtilTest, MemoryStreamTest)
{
}