//
// Created by Zach Lee on 2023/1/30.
//

#pragma once

#include "dx12/CommandBuffer.h"
#include "dx12/DevObject.h"

namespace sky::dx {

    class Device;

    class CommandPool : public DevObject {
    public:
        ~CommandPool() override = default;

        struct Descriptor {
        };

        bool Init(const Descriptor &desc);

        CommandBufferPtr Allocate(const CommandBuffer::Descriptor &);

    private:
        friend class Device;
        explicit CommandPool(Device &);

        ComPtr<ID3D12CommandAllocator> allocator;
        D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    };

    using CommandPoolPtr = std::shared_ptr<CommandPool>;
} // namespace sky::vk