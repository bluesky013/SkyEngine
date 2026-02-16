//
// Created by blues on 2024/11/23.
//

#pragma once

#include <core/async/ThreadPool.h>
#include <core/name/Name.h>
#include <core/environment/Singleton.h>
#include <core/async/Semaphore.h>

namespace sky {

    class NamedThread {
    public:
        explicit NamedThread(const Name &name = {});
        ~NamedThread();

        template <typename Func>
        void Dispatch(Func &&func)
        {
            executor.async(std::forward<Func>(func));
        }

        void Sync();
        void Signal();

    private:
        ThreadPool executor;
        Semaphore semaphore;
    };

} // namespace sky
