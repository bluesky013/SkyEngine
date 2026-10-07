//
// Created by blues on 2024/11/23.
//

#pragma once

#include <core/async/Semaphore.h>
#include <core/async/ThreadPool.h>
#include <core/name/Name.h>

#include <memory>
#include <utility>

namespace sky {

    // Single-worker thread with a name; Sync() blocks until the worker reaches a
    // Signal() point. Backed by the engine ThreadPool (no taskflow).
    class NamedThread {
    public:
        explicit NamedThread(const Name &name = {});
        ~NamedThread();

        template <typename Func>
        void Dispatch(Func &&func)
        {
            pool->Dispatch([f = std::forward<Func>(func)](ThreadContext &) mutable { f(); });
        }

        void Sync();
        void Signal();

    private:
        std::unique_ptr<ThreadPool> pool;
        Semaphore                   semaphore;
    };

} // namespace sky
