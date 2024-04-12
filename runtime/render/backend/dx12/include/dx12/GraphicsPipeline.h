//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <rhi/GraphicsPipeline.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class GraphicsPipeline : public rhi::GraphicsPipeline, public DevObject {
    public:
        explicit GraphicsPipeline(Device &dev);
        ~GraphicsPipeline() override = default;

    private:
        friend class Device;
        bool Init(const Descriptor&);

        ComPtr<ID3D12PipelineState> pipelineState;
    };

}