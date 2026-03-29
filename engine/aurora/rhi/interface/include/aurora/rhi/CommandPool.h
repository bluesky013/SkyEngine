//
// Created by blues on 2026/3/29.
//

#pragma once

#include <cstdint>
#include <memory>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    enum class CommandBufferLevel : uint8_t {
        PRIMARY = 0,
        SECONDARY
    };

    class CommandBuffer {
    public:
        CommandBuffer() = default;
        virtual ~CommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;
    };

    class CommandPool : public RefObject {
    public:
        CommandPool() = default;
        ~CommandPool() override = default;

        virtual void Reset() = 0;
        virtual CommandBuffer *Allocate(CommandBufferLevel level) = 0;
    };

    using CommandPoolPtr = CounterPtr<CommandPool>;

} // namespace sky::aurora
