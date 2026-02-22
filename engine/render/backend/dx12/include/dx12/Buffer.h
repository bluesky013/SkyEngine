//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/Buffer.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class Buffer : public rhi::Buffer, public DevObject {
    public:
        explicit Buffer(Device &dev);
        ~Buffer() override = default;

    private:
        bool Init(const Descriptor &);

        ComPtr<ID3D12Resource> resource;
    };
    using BufferPtr = std::shared_ptr<Buffer>;
}