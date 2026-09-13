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

    // Buffer kind tags: only declare the usage bits used to create the
    // underlying rhi buffer. Interpretation metadata (vertex layout, index
    // type) lives on the concrete buffer types, not here.
    struct VertexBufferKind {
        static constexpr BufferUsageFlags Usage = BufferUsageFlagBit::VERTEX;
    };
    struct IndexBufferKind {
        static constexpr BufferUsageFlags Usage = BufferUsageFlagBit::INDEX;
    };
    struct UniformBufferKind {
        static constexpr BufferUsageFlags Usage = BufferUsageFlagBit::UNIFORM;
    };
    struct StorageBufferKind {
        static constexpr BufferUsageFlags Usage =
            BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX | BufferUsageFlagBit::INDIRECT;
    };

    // Static tier: GPU_ONLY, uploaded once via a staging copy, immutable.
    template <typename Kind>
    class StaticBuffer : public RenderResource {
    public:
        StaticBuffer() = default;
        explicit StaticBuffer(const Name &inName) : RenderResource(inName) {}

        bool Init(Device *dev, uint64_t inSize)
        {
            device = dev;
            size   = inSize;
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
            queue->UploadBuffer(buffer.Get(), {request});
            return true;
        }

        Buffer  *GetBuffer() const { return buffer.Get(); }
        uint8_t *Map() { return nullptr; }

    protected:
        void Create() override
        {
            if (created || device == nullptr) {
                return;
            }
            Buffer::Descriptor desc;
            desc.size   = size;
            desc.usage  = Kind::Usage | BufferUsageFlagBit::TRANSFER_DST;
            desc.memory = MemoryType::GPU_ONLY;
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

        BufferPtr buffer;
        uint64_t  size = 0;
    };

    // Dynamic tier: CPU_TO_GPU, persistent-mapped, written every frame. Holds
    // a ring of numFramesInFlight buffers so a frame's write never races an
    // in-flight frame still reading the previous buffer; AdvanceFrame() cycles.
    template <typename Kind>
    class DynamicBuffer : public RenderResource {
    public:
        DynamicBuffer() = default;
        explicit DynamicBuffer(const Name &inName) : RenderResource(inName) {}

        bool Init(Device *dev, uint64_t inSize, uint32_t inFramesInFlight = 1)
        {
            device    = dev;
            size      = inSize;
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
                desc.usage  = Kind::Usage;
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
        uint64_t  size      = 0;
        uint32_t  numFrames = 1;
        uint32_t  current   = 0;
    };

    // Transient tier: per-frame scratch buffer. v1 allocates from the device
    // on first use (a dedicated TransientBufferPool is a follow-up).
    template <typename Kind>
    class TransientBuffer : public RenderResource {
    public:
        TransientBuffer() = default;
        explicit TransientBuffer(const Name &inName) : RenderResource(inName) {}

        bool Init(Device *dev, uint64_t inSize)
        {
            device = dev;
            size   = inSize;
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

        Buffer  *GetBuffer() const { return buffer.Get(); }

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
            desc.usage  = Kind::Usage;
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

        BufferPtr buffer;
        uint64_t  size = 0;
    };

    // One vertex binding: stride + semantics + input rate (maps to
    // VertexBindingDesc / VertexAttributeDesc).
    struct VertexLayout {
        uint32_t         stride    = 0;
        VertexInputRate  inputRate = VertexInputRate::PER_VERTEX;
        VertexSemanticMask semantics;
    };

    template <typename Storage = StaticBuffer<VertexBufferKind>>
    class VertexBuffer : public Storage {
    public:
        VertexBuffer() = default;
        explicit VertexBuffer(const Name &inName) : Storage(inName) {}

        const VertexLayout &GetLayout() const { return layout; }
        void SetLayout(const VertexLayout &inLayout) { layout = inLayout; }

    private:
        VertexLayout layout;
    };

    template <typename Storage = StaticBuffer<IndexBufferKind>>
    class IndexBuffer : public Storage {
    public:
        IndexBuffer() = default;
        explicit IndexBuffer(const Name &inName) : Storage(inName) {}

        IndexType GetIndexType() const { return indexType; }
        void SetIndexType(IndexType inType) { indexType = inType; }

    private:
        IndexType indexType = IndexType::NONE;
    };

    template <typename Storage = DynamicBuffer<UniformBufferKind>>
    class UniformBuffer : public Storage {
    public:
        UniformBuffer() = default;
        explicit UniformBuffer(const Name &inName) : Storage(inName) {}
    };

    // Generic buffer: can be bound as SSBO and re-interpreted as vertex /
    // index / indirect (e.g. a compute output consumed later). The binding
    // offset is supplied at bind time.
    template <typename Storage = DynamicBuffer<StorageBufferKind>>
    class StorageBuffer : public Storage {
    public:
        StorageBuffer() = default;
        explicit StorageBuffer(const Name &inName) : Storage(inName) {}

        Buffer *AsVertex(uint64_t offset) const
        {
            (void)offset;
            return this->GetBuffer();
        }
        Buffer *AsIndex(uint64_t offset) const
        {
            (void)offset;
            return this->GetBuffer();
        }
        Buffer *AsIndirect(uint64_t offset) const
        {
            (void)offset;
            return this->GetBuffer();
        }
    };

} // namespace sky::aurora
