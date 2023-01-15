//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <rhi/GraphicsPipeline.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class GraphicsPipeline : public rhi::GraphicsPipeline, public DevObject {
    public:
        GraphicsPipeline(Device &dev);
        ~GraphicsPipeline();

    private:
        friend class Device;
        bool Init(const Descriptor&);

        ComPtr<ID3D12PipelineState> pipelineState;
    };

}