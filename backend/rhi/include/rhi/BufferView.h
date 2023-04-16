//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <memory>

namespace sky::rhi {

    class BufferView {
    public:
        BufferView() = default;
        ~BufferView() = default;

        const BufferViewDesc &GetViewDesc() const { return viewDesc; }
        virtual uint8_t *Map() { return nullptr; }
        virtual void UnMap() {}

        virtual std::shared_ptr<BufferView> CreateView(const BufferViewDesc &) const = 0;

    protected:
        BufferViewDesc viewDesc;
    };
    using BufferViewPtr = std::shared_ptr<BufferView>;
}