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

    protected:
        BufferViewDesc viewDesc;
    };
    using BufferViewPtr = std::shared_ptr<BufferView>;
}