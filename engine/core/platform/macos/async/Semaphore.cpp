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
            uHandle = 0;
        }
    }

    void Semaphore::Wait()
    {
        semaphore_wait(uHandle);
    }

    void Semaphore::Signal(int32_t count)
    {
        for (int32_t i = 0; i < count; ++i) {
            // Do not retry on failure: after teardown (or if the semaphore was
            // destroyed) semaphore_signal returns KERN_TERMINATED/KERN_ABORTED and
            // a busy retry loop would spin forever.
            if (semaphore_signal(uHandle) != KERN_SUCCESS) {
                break;
            }
        }
    }
} // namespace sky
