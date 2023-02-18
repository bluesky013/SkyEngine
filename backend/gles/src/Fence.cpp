//
// Created by Zach Lee on 2023/2/2.
//

#include <gles/Fence.h>

namespace sky::gles {

    bool Fence::Init(const Descriptor &desc)
    {
        return true;
    }

    void Fence::Wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this]() { return ready; });
    }

    void Fence::Reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        ready = false;
    }

    void Fence::Signal()
    {
        std::lock_guard<std::mutex> lock(mutex);
        ready = true;
        cv.notify_all();
    }
}