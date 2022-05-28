//
// Created by Zach Lee on 2022/5/24.
//


#pragma once
#include <engine/render/resources/RenderResource.h>
#include <vulkan/Buffer.h>
#include <vector>

namespace sky {

    class Buffer : public RenderResource {
    public:
        struct Descriptor {
            VkDeviceSize        size      = 0;
            VkBufferUsageFlags  usage     = 0;
            VmaMemoryUsage      memory    = VMA_MEMORY_USAGE_CPU_TO_GPU;
            bool                useHost   = false;
        };

        Buffer(const Descriptor& desc);
        ~Buffer() = default;

        void InitRHI() override;

        bool IsValid() const override;

        void Update(const uint8_t* data, uint64_t size);

        void Update();

    protected:
        Descriptor descriptor;
        std::vector<uint8_t> rawData;
        drv::BufferPtr rhiBuffer;
    };
    using RDBufferPtr = std::shared_ptr<Buffer>;

    struct BufferView {
        RDBufferPtr buffer;
        uint32_t stride = 0;
        uint32_t offset = 0;
    };
}