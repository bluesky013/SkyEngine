//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/BufferView.h>
#include <dx12/DevObject.h>
#include <dx12/Buffer.h>

namespace sky::dx {

    class BufferView : public rhi::BufferView, public DevObject {
    public:
        BufferView(Device &dev);
        ~BufferView() override;

        std::shared_ptr<rhi::BufferView> CreateView(const rhi::BufferViewDesc &) const override;

    private:
        BufferPtr source;
    };

} // namespace sky::dx