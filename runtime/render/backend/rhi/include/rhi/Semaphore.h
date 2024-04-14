//
// Created by Zach Lee on 2023/2/19.
//

#pragma once
#include <memory>

namespace sky::rhi {

    class Semaphore {
    public:
        Semaphore() = default;
        ~Semaphore() = default;

        struct Descriptor {
        };
    };
    using SemaphorePtr = std::shared_ptr<Semaphore>;
}