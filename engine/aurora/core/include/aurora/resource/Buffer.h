//
// Created on 2026/09/13.
//

#pragma once

#include <aurora/resource/RenderResource.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Queue.h>
#include <aurora/rhi/VertexSemantic.h>
#include <core/name/Name.h>

#include <vector>

namespace sky::aurora {

    // Concrete IUploadStream over a raw host pointer. The pointed-to memory
    // must remain valid for the duration of the upload (uploads are
    // synchronous in v1).
    class RawBufferStream : public IUploadStream {
    public:
        RawBufferStream(const void *inData, uint64_t inSize)
            : data(static_cast<const uint8_t *>(inData))
            , size(inSize)
        {
        }

        const uint8_t *Data(uint64_t offset) override
        {
            return data + offset;
        }

        void ReadData(uint64_t offset, uint64_t inSize, uint8_t *out) override
        {
            for (uint64_t i = 0; i < inSize; ++i) {
                out[i] = data[offset + i];
            }
        }

    private:
        const uint8_t *data = nullptr;
        uint64_t       size = 0;
    };

    // Static tier: GPU_ONLY, uploaded once via a staging copy, immutable.
    class StaticBuffer : public RenderResource {
    public:
        StaticBuffer() = default;
        explicit StaticBuffer(const Name &inName) : RenderResource(inName) {}
        ~StaticBuffer() override { WaitUploadComplete(); }

        bool Init(Device *dev, uint64_t inSize, BufferUsageFlags inUsage)
        {
            device = dev;
            size   = inSize;
            usage  = inUsage;
            return dev != nullptr;
        }

        bool Upload(const void *data, uint64_t inSize, uint64_t offset = 0) override
        {
            if (device == nullptr) {
                return false;
            }
            if (!created) {
                Create();
            }
            if (!created) {
                return false;
            }

            auto *queue = device->GetQueue(QueueType::TRANSFER);
            if (queue == nullptr) {
                queue = device->GetQueue(QueueType::GRAPHICS);
            }
            if (queue == nullptr) {
                return false;
            }

            BufferUploadRequest request;
            request.source    = CounterPtr<IUploadStream>(new RawBufferStream(data, inSize));
            request.size      = inSize;
            request.dstOffset = offset;
            pendingHandle = queue->UploadBuffer(buffer.Get(), {request});
            pendingQueue  = queue;
            return true;
        }

        Buffer         *GetBuffer() const { return buffer.Get(); }
        BufferUsageFlags GetUsage() const { return usage; }
        uint8_t         *Map() { return nullptr; }

        // Async upload completion: query or block on the pending transfer task.
        bool IsUploadComplete() const
        {
            return pendingQueue == nullptr || pendingQueue->HasComplete(pendingHandle);
        }

        void WaitUploadComplete()
        {
            if (pendingQueue != nullptr) {
                pendingQueue->Wait(pendingHandle);
                pendingQueue = nullptr;
            }
        }

    protected:
        void Create() override
        {
            if (created || device == nullptr) {
                return;
            }
            Buffer::Descriptor desc;
            desc.size   = size;
            desc.usage  = usage | BufferUsageFlagBit::TRANSFER_DST;
            desc.memory = MemoryType::GPU_ONLY;
#if SKY_ENABLE_RESOURCE_NAME
            desc.name = name.GetStr().data();
#endif
            buffer  = BufferPtr(device->CreateBuffer(desc));
            created = buffer != nullptr;
        }

        void Release() override
        {
            WaitUploadComplete();
            buffer  = nullptr;
            created = false;
        }

        BufferPtr         buffer;
        uint64_t          size  = 0;
        BufferUsageFlags  usage = BufferUsageFlagBit::NONE;
        TransferTaskHandle pendingHandle = 0;
        Queue            *pendingQueue   = nullptr;
    };

    // Dynamic tier: CPU_TO_GPU, persistent-mapped, written every frame (the
    // "same-frame direct upload" mode). Holds a ring of framesInFlight buffers
    // so a frame's write never races an in-flight frame still reading the
    // previous buffer. framesInFlight must match DeviceFrameContext::inflightNum,
    // and the frame driver calls AdvanceFrame() once per frame after
    // DeviceFrameContext::BeginFrame(), keeping `current` in lockstep with the
    // frame context's mFrameIndex.
    class DynamicBuffer : public RenderResource {
    public:
        DynamicBuffer() = default;
        explicit DynamicBuffer(const Name &inName) : RenderResource(inName) {}

        bool Init(Device *dev, uint64_t inSize, BufferUsageFlags inUsage, uint32_t inFramesInFlight = 1)
        {
            device    = dev;
            size      = inSize;
            usage     = inUsage;
            numFrames = inFramesInFlight < 1 ? 1 : inFramesInFlight;
            buffers.resize(numFrames);
            return dev != nullptr;
        }

        bool Upload(const void *data, uint64_t inSize, uint64_t offset = 0) override
        {
            return Write(data, inSize, offset);
        }

        Buffer *GetBuffer() const
        {
            if (buffers.empty() || !created) {
                return nullptr;
            }
            return buffers[current].Get();
        }

        BufferUsageFlags GetUsage() const { return usage; }

        uint8_t *Map()
        {
            if (!created) {
                Create();
            }
            if (!created || buffers.empty()) {
                return nullptr;
            }
            return buffers[current]->Map();
        }

        bool Write(const void *data, uint64_t inSize, uint64_t offset = 0)
        {
            auto *mapped = Map();
            if (mapped == nullptr) {
                return false;
            }
            const auto *src = static_cast<const uint8_t *>(data);
            for (uint64_t i = 0; i < inSize; ++i) {
                mapped[offset + i] = src[i];
            }
            return true;
        }

        // Advance to the next in-flight frame's buffer (call once per frame).
        void AdvanceFrame()
        {
            if (buffers.empty()) {
                return;
            }
            current = (current + 1) % numFrames;
        }

    protected:
        void Create() override
        {
            if (created || device == nullptr) {
                return;
            }
            for (auto &buffer : buffers) {
                if (buffer != nullptr) {
                    continue;
                }
                Buffer::Descriptor desc;
                desc.size   = size;
                desc.usage  = usage;
                desc.memory = MemoryType::CPU_TO_GPU;
#if SKY_ENABLE_RESOURCE_NAME
                desc.name = name.GetStr().data();
#endif
                buffer = BufferPtr(device->CreateBuffer(desc));
            }
            created = true;
        }

        void Release() override
        {
            for (auto &buffer : buffers) {
                buffer = nullptr;
            }
            created = false;
        }

        std::vector<BufferPtr> buffers;
        uint64_t        size      = 0;
        uint32_t        numFrames = 1;
        uint32_t        current   = 0;
        BufferUsageFlags usage    = BufferUsageFlagBit::NONE;
    };

    // Transient tier: per-frame scratch buffer ("same-frame direct upload").
    // v1 allocates from the device on first use (a dedicated TransientBufferPool
    // is a follow-up).
    class TransientBuffer : public RenderResource {
    public:
        TransientBuffer() = default;
        explicit TransientBuffer(const Name &inName) : RenderResource(inName) {}

        bool Init(Device *dev, uint64_t inSize, BufferUsageFlags inUsage)
        {
            device = dev;
            size   = inSize;
            usage  = inUsage;
            return dev != nullptr;
        }

        bool Upload(const void *data, uint64_t inSize, uint64_t offset = 0) override
        {
            auto *mapped = Map();
            if (mapped == nullptr) {
                return false;
            }
            const auto *src = static_cast<const uint8_t *>(data);
            for (uint64_t i = 0; i < inSize; ++i) {
                mapped[offset + i] = src[i];
            }
            return true;
        }

        Buffer *GetBuffer() const { return buffer.Get(); }

        BufferUsageFlags GetUsage() const { return usage; }

        uint8_t *Map()
        {
            if (!created) {
                Create();
            }
            if (!created) {
                return nullptr;
            }
            return buffer->Map();
        }

    protected:
        void Create() override
        {
            if (created || device == nullptr) {
                return;
            }
            Buffer::Descriptor desc;
            desc.size   = size;
            desc.usage  = usage;
            desc.memory = MemoryType::CPU_TO_GPU;
#if SKY_ENABLE_RESOURCE_NAME
            desc.name = name.GetStr().data();
#endif
            buffer  = BufferPtr(device->CreateBuffer(desc));
            created = buffer != nullptr;
        }

        void Release() override
        {
            buffer  = nullptr;
            created = false;
        }

        BufferPtr       buffer;
        uint64_t        size  = 0;
        BufferUsageFlags usage = BufferUsageFlagBit::NONE;
    };

    // One vertex binding: stride + semantics + input rate (maps to
    // VertexBindingDesc / VertexAttributeDesc).
    struct VertexLayout {
        uint32_t         stride    = 0;
        VertexInputRate  inputRate = VertexInputRate::PER_VERTEX;
        VertexSemanticMask semantics;
    };

    class VertexBuffer : public StaticBuffer {
    public:
        VertexBuffer() = default;
        explicit VertexBuffer(const Name &inName) : StaticBuffer(inName) {}

        bool Init(Device *dev, uint64_t size)
        {
            return StaticBuffer::Init(dev, size, BufferUsageFlagBit::VERTEX);
        }

        const VertexLayout &GetLayout() const { return layout; }
        void SetLayout(const VertexLayout &inLayout) { layout = inLayout; }

    private:
        VertexLayout layout;
    };

    class IndexBuffer : public StaticBuffer {
    public:
        IndexBuffer() = default;
        explicit IndexBuffer(const Name &inName) : StaticBuffer(inName) {}

        bool Init(Device *dev, uint64_t size)
        {
            return StaticBuffer::Init(dev, size, BufferUsageFlagBit::INDEX);
        }

        IndexType GetIndexType() const { return indexType; }
        void SetIndexType(IndexType inType) { indexType = inType; }

    private:
        IndexType indexType = IndexType::NONE;
    };

    class UniformBuffer : public DynamicBuffer {
    public:
        UniformBuffer() = default;
        explicit UniformBuffer(const Name &inName) : DynamicBuffer(inName) {}

        bool Init(Device *dev, uint64_t size, uint32_t framesInFlight = 1)
        {
            return DynamicBuffer::Init(dev, size, BufferUsageFlagBit::UNIFORM, framesInFlight);
        }
    };

    // Generic buffer: can be bound as SSBO and re-interpreted as vertex /
    // index / indirect (e.g. a compute output consumed later). The binding
    // offset is supplied at bind time.
    class StorageBuffer : public DynamicBuffer {
    public:
        StorageBuffer() = default;
        explicit StorageBuffer(const Name &inName) : DynamicBuffer(inName) {}

        bool Init(Device *dev, uint64_t size, uint32_t framesInFlight = 1)
        {
            return DynamicBuffer::Init(dev, size,
                BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX | BufferUsageFlagBit::INDIRECT,
                framesInFlight);
        }

        Buffer *AsVertex(uint64_t offset) const
        {
            (void)offset;
            return GetBuffer();
        }
        Buffer *AsIndex(uint64_t offset) const
        {
            (void)offset;
            return GetBuffer();
        }
        Buffer *AsIndirect(uint64_t offset) const
        {
            (void)offset;
            return GetBuffer();
        }
    };

} // namespace sky::aurora
