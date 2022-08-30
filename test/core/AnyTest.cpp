//
// Created by Zach Lee on 2021/12/10.
//

#include <core/logger/Logger.h>
#include <core/type/Any.h>
#include <gtest/gtest.h>

using namespace sky;

static const char *TAG = "AnyTest";

struct Test1 {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
};

struct Test2 {
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
};

TEST(AnyTest, StorageTest)
{
    ASSERT_EQ(sizeof(Any), 32);

    Any any1(std::in_place_type<Test1>, 1u, 2u, 3u, 4u, 5u, 6u);
    ASSERT_EQ(&any1, any1.Data());

    Any any2(std::in_place_type<Test2>, 1ull, 2ull, 3ull, 4ull);
    ASSERT_NE(&any2, any2.Data());

    Any any3(std::in_place_type<float>, 5.f);
    ASSERT_EQ(&any3, any3.Data());
}

static uint32_t g_Counter = 0;
class AnyTestCls1 {
public:
    AnyTestCls1()
    {
        g_Counter++;
    }

    ~AnyTestCls1()
    {
        g_Counter--;
    }
};

class AnyTestCls2 {
public:
    AnyTestCls2()
    {
        g_Counter++;
    }

    ~AnyTestCls2()
    {
        g_Counter--;
    }

    char data[64];
};

TEST(AnyTest, CtorDtorTest)
{
    {
        Any any(std::in_place_type<AnyTestCls1>);
        ASSERT_EQ(g_Counter, 1);
    }
    ASSERT_EQ(g_Counter, 0);

    {
        Any any(std::in_place_type<AnyTestCls2>);
        ASSERT_EQ(g_Counter, 1);
    }
    ASSERT_EQ(g_Counter, 0);
}

struct AnyCopy1 {
    AnyCopy1() : a(0)
    {
    }

    AnyCopy1(uint32_t v) : a(v)
    {
    }

    ~AnyCopy1()
    {
    }

    AnyCopy1(const AnyCopy1 &any)
    {
        a = any.a;
    }

    AnyCopy1(AnyCopy1 &&any)
    {
        a = any.a;
    }

    uint32_t a;
};

struct AnyCopy2 {

    AnyCopy2() : a(0)
    {
    }

    AnyCopy2(uint32_t v) : a(v)
    {
    }

    ~AnyCopy2()
    {
        a = 0;
    }

    AnyCopy2(const AnyCopy2 &any)
    {
        a = any.a;
    }

    AnyCopy2(AnyCopy2 &&any)
    {
        a = any.a;
    }

    uint32_t a;
    char     data[32];
};

TEST(AnyTest, CopyMoveTest)
{
    Any any1(std::in_place_type<AnyCopy1>, 1u);
    {
        AnyCopy1 *ptr = any1.GetAs<AnyCopy1>();
        ASSERT_EQ(ptr->a, 1);
    }
    {
        Any       tmp = any1;
        AnyCopy1 *ptr = tmp.GetAs<AnyCopy1>();
        ASSERT_NE(ptr, nullptr);
        ASSERT_EQ(ptr->a, 1);
    }

    Any any2(std::in_place_type<AnyCopy2>, 1u);
    {
        AnyCopy2 *ptr = any2.GetAs<AnyCopy2>();
        ASSERT_EQ(ptr->a, 1);
    }
    {
        Any       tmp(std::move(any2));
        AnyCopy2 *ptr1 = tmp.GetAs<AnyCopy2>();
        ASSERT_NE(ptr1, nullptr);
        ASSERT_EQ(ptr1->a, 1);

        AnyCopy2 *ptr2 = any2.GetAs<AnyCopy2>();
        ASSERT_EQ(ptr2, nullptr);
    }
}

TEST(AnyTest, AnyRefTest)
{
    Test1 t1 = {1, 2, 3, 4, 5, 6};
    Test2 t2 = {1, 2, 3, 4};

    Any rfa1(std::ref(t1));
    Any rfa2(std::ref(t2));

    Test1 *ref1 = *rfa1.GetAs<Test1 *>();
    Test2 *ref2 = *rfa2.GetAs<Test2 *>();

    ref1->a = 99;
    ref2->a = 99;

    ref1->f = 101;
    ref2->d = 101;

    ASSERT_EQ(t1.a, 99);
    ASSERT_EQ(t1.f, 101);

    ASSERT_EQ(t2.a, 99);
    ASSERT_EQ(t2.d, 101);
}
