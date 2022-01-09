//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <map>
#include <vector>

namespace sky::drv {

    class Device;

    class PipelineLayout : public DevObject {
    public:
        ~PipelineLayout();

        struct Descriptor {
            std::map<uint32_t, uint32_t> desLayouts;
            std::vector<VkPushConstantRange> pushConstants;
        };

        bool Init(const Descriptor&);

        VkPipelineLayout GetNativeHandle() const;

        uint32_t GetHash() const;

        const std::vector<uint32_t>& GetRequirements() const;

    private:
        friend class Device;
        PipelineLayout(Device&);

        VkPipelineLayout layout;
        uint32_t hash;
        Descriptor descriptor;
        std::vector<uint32_t> requirements;
    };

    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;

}