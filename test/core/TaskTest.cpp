//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>
#include <gtest/gtest.h>

using namespace sky;

class TestTask : public Task {
public:
    explicit TestTask()  = default;
    ~TestTask() override
    {
        printf("test\n");
    }

    bool DoWork() override
    {
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(10));
        return true;
    }
};

class TestTaskCallback : public ITaskCallBack {
public:
    explicit TestTaskCallback(uint32_t &v) : id(v) {}
    ~TestTaskCallback() = default;

    void OnTaskComplete(bool result, Task *task)
    {
        id = 20;
    }

    uint32_t &id;
};

TEST(TaskTest, TaskTestBase)
{
    uint32_t id1 = 0;
    uint32_t id2 = 0;
    {
        TestTaskCallback     test(id1);
        CounterPtr<TestTask> task = new TestTask();
        task->SetCallback(&test);
        task->StartAsync();
    }
    TaskExecutor::Get()->GetExecutor().wait_for_all();
    ASSERT_EQ(id1, 0);

    TestTaskCallback     test1(id2);
    CounterPtr<TestTask> task = new TestTask();
    task->SetCallback(&test1);
    task->StartAsync();
    TaskExecutor::Get()->GetExecutor().wait_for_all();
    ASSERT_EQ(id2, 20);
}