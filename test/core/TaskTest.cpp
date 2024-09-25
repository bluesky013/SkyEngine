//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>
#include <gtest/gtest.h>

using namespace sky;

class TestTask : public Task {
public:
    explicit TestTask(uint32_t &v) : id(v) {}
    ~TestTask() override = default;

    bool DoWork() override
    {
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(10));
        return true;
    }

    void OnComplete(bool result) override
    {
        id = 20;
    }

private:
    uint32_t& id;
};

TEST(TaskTest, TaskTestBase)
{
    uint32_t id = 0;

    auto *tf = TaskExecutor::Get();
    tf->ExecuteTask(new TestTask(id));
    tf->WaitForAll();

    ASSERT_EQ(id, 20);
}