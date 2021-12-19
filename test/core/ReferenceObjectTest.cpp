//
// Created by Zach Lee on 2021/12/19.
//

#include <gtest/gtest.h>
#include <core/template/ReferenceObject.h>
using namespace sky;

static uint32_t g_count = 0;

struct TestRef : public RefObject<TestRef> {
public:
    TestRef()
    {

    }

    ~TestRef()
    {
        g_count++;
    }

    TestRef(const TestRef&)
    {

    }

    TestRef& operator=(const TestRef&)
    {
        return *this;
    }

    void OnExpire() override
    {
        delete this;
    }
};

struct TestRefDev : public TestRef {
public:
    TestRefDev() = default;
    ~TestRefDev() = default;
};

TEST(ReferenceObjectTest, ConstructTest)
{
    auto ptr = new TestRefDev();

    {
        CounterPtr<TestRef> cPtr1(ptr);
        ASSERT_EQ(ptr->GetRef(), 1);

        CounterPtr<TestRefDev> cPtr2(ptr);
        ASSERT_EQ(ptr->GetRef(), 2);

        CounterPtr<TestRef> cPtr3 = cPtr2;
        ASSERT_EQ(ptr->GetRef(), 3);
    }
    ASSERT_EQ(g_count, 1);
}