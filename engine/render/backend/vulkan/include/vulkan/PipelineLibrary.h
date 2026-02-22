//
// Created by blues on 2023/10/13.
//

#pragma once

#include <rhi/PipelineLibrary.h>
#include <vulkan/DevObject.h>

namespace sky::vk {
    class Device;

    class PipelineLibrary : public rhi::PipelineLibrary, public DevObject{
    public:
        explicit PipelineLibrary(Device &dev) : DevObject(dev) {}
        ~PipelineLibrary() override;


    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        VkPipelineCache cache = VK_NULL_HANDLE;
    };

} // namespace sky::vk
