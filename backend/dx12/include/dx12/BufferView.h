//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/BufferView.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class BufferView : public rhi::BufferView, public DevObject {
    public:
        BufferView(Device &dev);
        ~BufferView() override;
    };

}