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
        explicit BufferView(Device &dev) : DevObject(dev) {}
        ~BufferView() override = default;

        bool Init(const rhi::BufferViewDesc &desc);

        GLuint GetNativeHandle() const { return source->GetNativeHandle(); }
        const BufferPtr &GetBuffer() const { return source; }

        std::shared_ptr<rhi::BufferView> CreateView(const rhi::BufferViewDesc &) const override;

    private:
        friend class Buffer;
        BufferPtr source;
    };
    using BufferViewPtr = std::shared_ptr<BufferView>;

}

