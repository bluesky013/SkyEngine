//
// Created by Zach Lee on 2021/12/9.
//

#include <gtest/gtest.h>
#include <framework/serialization/Any.h>

using namespace sky;

struct Test1 {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
};

struct Test2 {
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
};

TEST(SerializationTest, AnyTest)
{
    Any any1(std::in_place_type<Test1>, 1u, 2u, 3u, 4u, 5u, 6u, 7u);
    ASSERT_EQ(&any1, any1.Data());

    Any any2(std::in_place_type<Test2>, 1ull, 2ull, 3ull, 4ull);
    ASSERT_NE(&any2, any2.Data());

    Any any3(std::in_place_type<float>, 5.f);
    ASSERT_EQ(&any3, any3.Data());
}