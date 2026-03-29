//
// Created by blues on 2026/3/29.
//

#pragma once

#include <cstdint>
#include <string>
#include <aurora/rhi/CommandPool.h>

namespace sky::aurora {

    enum class QueueType : uint32_t {
        GRAPHICS = 0,
        COMPUTE,
        TRANSFER,
    };

    class Device {
    public:
        Device() = default;
        virtual ~Device() = default;

        virtual bool Init() = 0;
        virtual std::string GetDeviceInfo() const { return ""; }
        virtual void WaitIdle() const = 0;

        virtual CommandPool *CreateCommandPool(QueueType type) = 0;
    };

} // namespace sky::aurora
