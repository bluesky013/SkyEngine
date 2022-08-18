//
// Created by Zach Lee on 2022/2/1.
//

#pragma once
#include <render/resources/Buffer.h>
#include <render/RenderConstants.h>
#include <vector>

namespace sky {

    class RenderBufferPool;

    class DynamicBufferView : public BufferView {
    public:
        DynamicBufferView(RDBufferPtr buffer, VkDeviceSize size, VkDeviceSize offset, uint32_t frame, uint32_t block);
        ~DynamicBufferView() = default;

        uint32_t GetDynamicOffset() const;

        void SetID(uint32_t);

        uint32_t GetID() const;

        virtual void RequestUpdate() override;

        virtual void WriteImpl(const uint8_t* data, uint64_t size, uint64_t offset) override;

    private:
        friend class RenderBufferPool;

        void SwapBuffer();

        const uint32_t frameNum;
        const uint32_t blockStride;
        uint32_t bufferIndex;
        uint32_t dynamicOffset;
        uint32_t currentFrame;
        bool isDirty;
    };
    using RDDynBufferViewPtr = std::shared_ptr<DynamicBufferView>;

    class RenderBufferPool {
    public:
        struct Descriptor {
            uint32_t            count     = 1;
            uint32_t            stride    = 4;
            uint32_t            frame     = INFLIGHT_FRAME;
            VkBufferUsageFlags  usage     = 0;
            VmaMemoryUsage      memory    = VMA_MEMORY_USAGE_UNKNOWN;
        };

        RenderBufferPool(const Descriptor& desc);
        ~RenderBufferPool();

        RDDynBufferViewPtr Allocate();

        void Free(uint32_t index);

        void Update();

    private:
        void AllocateBlock();

        Descriptor descriptor;
        uint32_t blockStride = 0;
        std::vector<RDBufferPtr> blocks;
        std::list<uint32_t> active;
        std::list<uint32_t> freeList;
    };

}
