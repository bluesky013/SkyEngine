//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <dx12/DevObject.h>

namespace sky::dx {

    class GraphicsPipeline : public DevObject {
    public:
        GraphicsPipeline(Device &dev);
        ~GraphicsPipeline();

        struct Descriptor {
        };

    private:
        friend class Device;
        bool Init(const Descriptor&);

        ComPtr<ID3D12PipelineState> pipelineState;
    };

}