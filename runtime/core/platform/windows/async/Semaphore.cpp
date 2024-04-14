//
// Created by Zach Lee on 2023/2/2.
//

#include <core/async/Semaphore.h>
#include <windows.h>

namespace sky {

    Semaphore::Semaphore(int initial)
    {
        auto semaphore = CreateSemaphore(NULL, initial, 0x7fffffff, NULL);
        handle = static_cast<void*>(semaphore);
    }

    Semaphore::~Semaphore()
    {
        CloseHandle(handle);
    }

    void Semaphore::Wait()
    {
        WaitForSingleObject(handle, 0xffffffff);
    }

    void Semaphore::Signal(int32_t count)
    {
        ReleaseSemaphore(handle, count, nullptr);
    }

}
