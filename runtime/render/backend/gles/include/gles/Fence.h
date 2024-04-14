//
// Created by Zach Lee on 2023/2/2.
//

#pragma once

#include <rhi/Fence.h>
#include <gles/DevObject.h>
#include <condition_variable>
#include <mutex>

namespace sky::gles {

    class Fence : public rhi::Fence, public DevObject {
    public:
        Fence(Device &dev) : DevObject(dev) {}
        ~Fence() = default;

        bool Init(const Descriptor &desc);

        void Wait() override;
        void Reset() override;
        void Signal();

    private:
        bool ready = true;
        mutable std::mutex mutex;
        mutable std::condition_variable cv;
    };
    using FencePtr = std::shared_ptr<Fence>;
}