//
// Created by Zach Lee on 2021/12/19.
//

#include <core/template/ReferenceObject.h>
#include <gtest/gtest.h>
using namespace sky;

static uint32_t g_count = 0;

class TestRef : public RefObject {
public:
    TestRef() = default;

    ~TestRef() override
    {
        g_count++;
    }
};

class TestRefDev : public TestRef {
public:
    TestRefDev()  = default;
    ~TestRefDev() override = default;
};

TEST(ReferenceObjectTest, ConstructTest)
{
    {
        auto *ptr = new TestRefDev();

        CounterPtr<TestRefDev> cPtr1(ptr);
        ASSERT_EQ(ptr->GetRef(), 1);

        CounterPtr<TestRefDev> cPtr2(cPtr1);
        ASSERT_EQ(ptr->GetRef(), 2);

        CounterPtr<TestRefDev> cPtr3(std::move(cPtr2));
        ASSERT_EQ(ptr->GetRef(), 2);
    }
    ASSERT_EQ(g_count, 1);

    {
        CounterPtr<TestRef> ptr = new TestRef();
        ptr = new TestRef();
    }
    ASSERT_EQ(g_count, 3);
}