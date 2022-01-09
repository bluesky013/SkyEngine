//
// Created by Zach Lee on 2022/1/9.
//

#pragma once

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Device;

    class GraphicsPipeline : public DevObject {
    public:
        ~GraphicsPipeline() = default;

        struct Descriptor {
        };

        bool Init(const Descriptor&);

        VkPipeline GetNativeHandle() const;

    private:
        friend class Device;
        GraphicsPipeline(Device&);

        VkPipeline pipeline;
        uint32_t hash;
    };

    using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;

}