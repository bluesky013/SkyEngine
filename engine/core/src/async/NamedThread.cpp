//
// Created by blues on 2024/11/23.
//

#include <core/async/NamedThread.h>

namespace sky {
    namespace impl {
        void SetCurrentThreadName(const std::string_view &name);
    }

    NamedThread::NamedThread(const Name &name) : pool(std::make_unique<ThreadPool>(1)), semaphore(1)
    {
        Dispatch([name]() { impl::SetCurrentThreadName(name.GetStr()); });
    }

    NamedThread::~NamedThread()
    {
        // Queued Signal callbacks borrow the semaphore, so the pool must drain before member destruction.
        pool.reset();
    }

    void NamedThread::Sync()
    {
        semaphore.Wait();
    }

    void NamedThread::Signal()
    {
        Dispatch([this]() { semaphore.Signal(); });
    }

} // namespace sky
