//
// Created by Zach Lee on 2022/6/22.
//
#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <framework/interface/Event.h>

using namespace sky;

struct ITestEvent1 : public EventTrait {
    virtual void E1() = 0;
    virtual void E2(float) = 0;
};

struct ITestEvent2 : public EventTrait {
    using KeyType = int;

    virtual void E3() = 0;
    virtual void E4(float) = 0;
};

struct EventListener : public ITestEvent1, public ITestEvent2 {
    EventListener()
    {
        Event<ITestEvent1>::Connect(this);
    }

    ~EventListener()
    {
        Event<ITestEvent1>::DisConnect(this);
    }

    void Connect(int v)
    {
        Event<ITestEvent2>::Connect(v, this);
    }

    void DisConnect(int v)
    {
        Event<ITestEvent2>::DisConnect(v, this);
    }

    void E1()
    {
        a = 1;
    }

    void E2(float val) override
    {
        b = val;
    }

    void E3() override
    {
        a = 3;
    }

    void E4(float val) override
    {
        b = val;
    }

    int a = 0;
    float b = 0;
};

TEST(EventTest, BroadCastTest)
{
    EventListener listener1;
    EventListener listener2;

    Event<ITestEvent1>::BroadCast(&ITestEvent1::E1);
    ASSERT_EQ(listener1.a, 1);
    ASSERT_EQ(listener2.a, 1);

    Event<ITestEvent1>::BroadCast(&ITestEvent1::E2, 1.f);
    ASSERT_EQ(listener1.b, 1.f);
    ASSERT_EQ(listener2.b, 1.f);

    Event<ITestEvent1>::BroadCast(&ITestEvent1::E2, 2.f);
    ASSERT_EQ(listener1.b, 2.f);
    ASSERT_EQ(listener2.b, 2.f);

    listener1.Connect(1);
    listener2.Connect(2);

    Event<ITestEvent2>::BroadCast(1, &ITestEvent2::E3);
    ASSERT_EQ(listener1.a, 3);
    ASSERT_EQ(listener2.a, 1);

    Event<ITestEvent2>::BroadCast(2, &ITestEvent2::E3);
    ASSERT_EQ(listener1.a, 3);
    ASSERT_EQ(listener2.a, 3);

    Event<ITestEvent2>::BroadCast(1, &ITestEvent2::E4, 3.f);
    ASSERT_EQ(listener1.b, 3.f);
    ASSERT_EQ(listener2.b, 2.f);

    Event<ITestEvent2>::BroadCast(2, &ITestEvent2::E4, 4.f);
    ASSERT_EQ(listener1.b, 3.f);
    ASSERT_EQ(listener2.b, 4.f);

}