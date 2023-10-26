//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/BufferView.h>
#include <mtl/DevObject.h>
#include <mtl/Buffer.h>

namespace sky::mtl {

    class Device;

    class BufferView : public rhi::BufferView, public DevObject {
    public:
        BufferView(Device &dev) : DevObject(dev) {}
        ~BufferView() = default;

        std::shared_ptr<rhi::BufferView> CreateView(const rhi::BufferViewDesc &) const override;

        id<MTLBuffer> GetNativeHandle() const { return source->GetNativeHandle(); }
    private:
        friend class Buffer;
        bool Init(const rhi::BufferViewDesc &desc);

        BufferPtr source;
    };
    using BufferViewPtr = std::shared_ptr<BufferView>;
}
