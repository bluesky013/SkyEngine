//
// Created by blues on 2024/11/23.
//

#include <core/async/NamedThread.h>

namespace sky {
    namespace impl {
        void SetCurrentThreadName(const std::string_view &name);
    }

    NamedThread::NamedThread(const Name& name)
        : executor(1)
        , semaphore(1)
    {
        Dispatch([name]() { impl::SetCurrentThreadName(name.GetStr()); });
    }

    NamedThread::~NamedThread() = default;

    void NamedThread::Sync()
    {
        semaphore.Wait();
    }

    void NamedThread::Signal()
    {
        Dispatch([this]() { semaphore.Signal(); });
    }

} // namespace sky