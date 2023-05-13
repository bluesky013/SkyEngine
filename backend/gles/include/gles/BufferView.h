//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/BufferView.h>
#include <gles/DevObject.h>
#include <gles/Buffer.h>

namespace sky::gles {

    class BufferView : public rhi::BufferView, public DevObject {
    public:
        BufferView(Device &dev) : DevObject(dev) {}
        ~BufferView() = default;

        bool Init(const rhi::BufferViewDesc &desc);

        GLuint GetNativeHandle() const { return source->GetNativeHandle(); }

        std::shared_ptr<rhi::BufferView> CreateView(const rhi::BufferViewDesc &) const override;

    private:
        friend class Buffer;
        BufferPtr source;
    };
    using BufferViewPtr = std::shared_ptr<BufferView>;

}

