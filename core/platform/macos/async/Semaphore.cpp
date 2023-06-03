//
// Created by Zach Lee on 2023/6/1.
//


#include <core/async/Semaphore.h>

#include <mach/mach.h>

namespace sky {

    Semaphore::Semaphore(int initial)
    {
        semaphore_create(mach_task_self(), &uHandle, SYNC_POLICY_FIFO, initial);
    }

    Semaphore::~Semaphore()
    {
        if (uHandle != 0) {
            semaphore_destroy(mach_task_self(), uHandle);
        }
    }

    void Semaphore::Wait()
    {
        semaphore_wait(uHandle);
    }

    void Semaphore::Signal(int32_t count)
    {
        while(count-- > 0) {
            while (semaphore_signal(uHandle) != KERN_SUCCESS);
        }
    }
} // namespace sky
