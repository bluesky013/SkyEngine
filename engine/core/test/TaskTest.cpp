//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>

using namespace sky;

class TestTask : public Task {
public:
    explicit TestTask(uint32_t &v) : value(v)
    {
    }
    ~TestTask() override = default;

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
    uint32_t             id   = 0;
    CounterPtr<TestTask> task = new TestTask(id);
    ASSERT_EQ(id, 0);
    task->StartAsync();
    TaskExecutor::Get()->WaitForAll();
    ASSERT_EQ(id, 20);
}

namespace {
    class LifetimeTask final : public Task {
    public:
        LifetimeTask(std::shared_ptr<int> resource, std::atomic_bool &completed, bool result)
            : resource(std::move(resource)), completed(completed), result(result)
        {
        }

    private:
        bool DoWork() override
        {
            return result;
        }
        void OnComplete(bool value) override
        {
            completed.store(value == result);
        }

        std::shared_ptr<int> resource;
        std::atomic_bool    &completed;
        bool                 result;
    };
} // namespace

TEST(TaskTest, CompletedWorkReleasesTaskWhileDependencyHandleRemains)
{
    for (const bool result : {true, false}) {
        auto                     resource = std::make_shared<int>(1);
        const std::weak_ptr<int> observer = resource;
        std::atomic_bool         completed{false};
        CounterPtr<LifetimeTask> task = new LifetimeTask(std::move(resource), completed, result);
        task->StartAsync();
        const auto handle = task->GetTask();
        task              = nullptr;
        TaskExecutor::Get()->WaitForAll();

        EXPECT_TRUE(completed.load());
        EXPECT_TRUE(observer.expired());
        EXPECT_EQ(handle->GetFuture().wait_for(std::chrono::seconds(0)), std::future_status::ready);
    }
}

TEST(TaskTest, CallerCanKeepCompletedTaskAndRestartIt)
{
    auto                     resource = std::make_shared<int>(1);
    const std::weak_ptr<int> observer = resource;
    std::atomic_bool         completed{false};
    CounterPtr<LifetimeTask> task = new LifetimeTask(std::move(resource), completed, true);
    task->StartAsync();
    const auto firstHandle = task->GetTask();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_TRUE(completed.load());
    EXPECT_FALSE(observer.expired());

    completed.store(false);
    task->StartAsync();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_TRUE(completed.load());
    EXPECT_FALSE(task->IsWorking());
    task = nullptr;
    EXPECT_TRUE(observer.expired());
    EXPECT_EQ(firstHandle->GetFuture().wait_for(std::chrono::seconds(0)), std::future_status::ready);
}
