//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>
#include <gtest/gtest.h>

using namespace sky;

class TestTask : public Task {
public:
    explicit TestTask(uint32_t &v) : value(v)
    {
    }
    ~TestTask() override
    {
        printf("test\n");
    }

    bool DoWork() override
    {
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(10));
        value = 20;
        return true;
    }

private:
    uint32_t &value;
};

TEST(TaskTest, TaskTestBase)
{
    uint32_t id = 0;
    CounterPtr<TestTask> task = new TestTask(id);
    ASSERT_EQ(id, 0);
    task->StartAsync();
    TaskExecutor::Get()->GetExecutor().wait_for_all();
    ASSERT_EQ(id, 20);
}