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

TEST(TaskTest, ParallelForBasic)
{
    const int N = 100;
    std::vector<int> v(N, 0);

    ThreadPool pool(4);
    pool.parallel_for(0, N, [&v](int i) {
        v[i] = i * 2;
    });

    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(v[i], i * 2);
    }
}

TEST(TaskTest, ParallelForEmpty)
{
    ThreadPool pool(4);
    // Should not crash or hang
    pool.parallel_for(0, 0, [](int) {
        FAIL() << "Should not be called";
    });

    pool.parallel_for(5, 5, [](int) {
        FAIL() << "Should not be called";
    });

    pool.parallel_for(10, 5, [](int) {
        FAIL() << "Should not be called";
    });
}

TEST(TaskTest, ParallelForSingleElement)
{
    int value = 0;
    ThreadPool pool(4);
    pool.parallel_for(0, 1, [&value](int i) {
        value = 42;
    });
    ASSERT_EQ(value, 42);
}

TEST(TaskTest, ParallelForLargeRange)
{
    const int N = 10000;
    std::vector<int> v(N, 0);

    ThreadPool pool(4);
    pool.parallel_for(0, N, [&v](int i) {
        v[i] = i;
    });

    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(v[i], i);
    }
}

TEST(TaskTest, ParallelForConvenience)
{
    const int N = 100;
    std::vector<int> v(N, 0);

    ParallelFor(0, N, [&v](int i) {
        v[i] = i + 1;
    });

    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(v[i], i + 1);
    }
}