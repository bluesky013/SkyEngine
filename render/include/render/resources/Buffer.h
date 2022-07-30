//
// Created by Zach Lee on 2022/5/24.
//


#pragma once
#include <render/resources/RenderResource.h>
#include <vulkan/Buffer.h>
#include <vector>

namespace sky {

    class Buffer : public RenderResource {
    public:
        struct Descriptor {
            VkDeviceSize        size    = 0;
            VkBufferUsageFlags  usage   = 0;
            VmaMemoryUsage      memory  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            bool                keepCPU = false;
        };

        Buffer(const Descriptor& desc);
        ~Buffer() = default;

        void InitRHI() override;

        bool IsValid() const override;

        void Write(const uint8_t* data, uint64_t size, uint64_t offset = 0);

        template <typename T>
        void Write(const T& value, uint64_t offset)
        {
            Write(&value, sizeof(T), offset);
        }

        void Update(const uint8_t* data, uint64_t size);

        void Update(bool release = false);

    protected:
        Descriptor descriptor;
        std::vector<uint8_t> rawData;
        drv::BufferPtr rhiBuffer;
    };
    using RDBufferPtr = std::shared_ptr<Buffer>;

    class BufferView : public RenderResource {
    public:
        BufferView(RDBufferPtr buffer, uint32_t stride, uint32_t offset);
        ~BufferView() = default;

    private:
        RDBufferPtr buffer;
        uint32_t stride = 0;
        uint32_t offset = 0;
    };
    using RDBufferViewPtr = std::shared_ptr<BufferView>;
}