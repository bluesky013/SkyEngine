#include <core/async/Semaphore.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

TEST(SemaphoreTest, WaitBlocksUntilSignaled)
{
    sky::Semaphore     semaphore;
    std::promise<void> entered;
    std::promise<void> completed;
    auto               ready = entered.get_future();
    auto               done  = completed.get_future();
    std::thread        worker([&]() {
        entered.set_value();
        semaphore.Wait();
        completed.set_value();
    });
    ready.wait();
    EXPECT_EQ(done.wait_for(std::chrono::milliseconds(30)), std::future_status::timeout);
    semaphore.Signal();
    EXPECT_EQ(done.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    worker.join();
}

TEST(SemaphoreTest, SignalCountReleasesExactlyThatManyWaiters)
{
    sky::Semaphore           semaphore;
    std::mutex               mutex;
    std::condition_variable  changed;
    uint32_t                 entered   = 0;
    uint32_t                 completed = 0;
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < 3; ++i) {
        workers.emplace_back([&]() {
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++entered;
                changed.notify_all();
            }
            semaphore.Wait();
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++completed;
                changed.notify_all();
            }
        });
    }
    {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [&]() { return entered == 3; });
        semaphore.Signal(2);
        EXPECT_TRUE(changed.wait_for(lock, std::chrono::seconds(2), [&]() { return completed >= 2; }));
        EXPECT_FALSE(changed.wait_for(lock, std::chrono::milliseconds(30), [&]() { return completed > 2; }));
        EXPECT_EQ(completed, 2u);
    }
    semaphore.Signal();
    for (auto &worker : workers) {
        worker.join();
    }
    EXPECT_EQ(completed, 3u);
}

TEST(SemaphoreTest, InitialPermitsAndSignalBeforeWaitAreRetained)
{
    sky::Semaphore semaphore(2);
    semaphore.Wait();
    semaphore.Wait();
    semaphore.Signal(3);
    semaphore.Wait();
    semaphore.Wait();
    semaphore.Wait();
}
