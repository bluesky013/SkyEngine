//
// Created by Zach Lee on 2022/5/24.
//


#pragma once

#include <framework/asset/Asset.h>
#include <framework/serialization/BasicSerialization.h>
#include <render/resources/RenderResource.h>
#include <vulkan/Buffer.h>
#include <vector>

namespace sky {

    struct BufferAssetData {
        std::vector<uint8_t> data;
        VkBufferUsageFlags  usage    = 0;
        VmaMemoryUsage      memory   = VMA_MEMORY_USAGE_CPU_TO_GPU;

        template<class Archive>
        void serialize(Archive &ar)
        {
            ar(data, usage, memory);
        }
    };

    class Buffer : public RenderResource {
    public:
        struct Descriptor {
            VkDeviceSize        size     = 0;
            VkBufferUsageFlags  usage    = 0;
            VmaMemoryUsage      memory   = VMA_MEMORY_USAGE_CPU_TO_GPU;
            bool                allocCPU = false;
            bool                keepMap  = false;
        };

        Buffer() = default;
        Buffer(const Descriptor& desc);
        ~Buffer();

        void Init(const Descriptor& desc);

        void InitRHI();

        bool IsValid() const override;

        uint8_t *GetMappedAddress() const;

        void Write(const uint8_t* data, uint64_t size, uint64_t offset = 0);

        template <typename T>
        void Write(const T& value, uint64_t offset)
        {
            Write(reinterpret_cast<const uint8_t*>(&value), sizeof(T), offset);
        }

        void Update(const uint8_t* data, uint64_t size);

        void Update(bool release = false);

        drv::BufferPtr GetRHIBuffer() const;

        const uint8_t* Data() const;

    protected:
        Descriptor descriptor;
        std::vector<uint8_t> rawData;
        drv::BufferPtr rhiBuffer;
        uint8_t* mapPtr = nullptr;
        bool dirty = true;
    };
    using RDBufferPtr = std::shared_ptr<Buffer>;

    class BufferView : public RenderResource {
    public:
        BufferView(RDBufferPtr buffer, VkDeviceSize size, VkDeviceSize offset, uint32_t stride = 0);
        ~BufferView() = default;

        RDBufferPtr GetBuffer() const;

        VkDeviceSize GetOffset() const;

        VkDeviceSize GetSize() const;

        uint32_t GetStride() const;

        bool IsValid() const;

        virtual void RequestUpdate();

        template <typename T>
        void Write(const T& value, uint64_t off = 0)
        {
            WriteImpl(reinterpret_cast<const uint8_t*>(&value), sizeof(T), offset);
        }

    protected:
        virtual void WriteImpl(const uint8_t* data, uint64_t size, uint64_t offset);

        RDBufferPtr buffer;
        VkDeviceSize size = 0;
        VkDeviceSize offset = 0;
        uint32_t stride = 0;
    };
    using RDBufferViewPtr = std::shared_ptr<BufferView>;

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

    namespace impl {
        void LoadFromPath(const std::string& path, BufferAssetData& data);
        void SaveToPath(const std::string& path, const BufferAssetData& data);
        RDBufferPtr CreateFromData(const BufferAssetData& data);
    }

    template <>
    struct AssetTraits <Buffer> {
        using DataType = BufferAssetData;

        static void LoadFromPath(const std::string& path, DataType& data)
        {
            impl::LoadFromPath(path, data);
        }

        static RDBufferPtr CreateFromData(const DataType& data)
        {
            return impl::CreateFromData(data);
        }

        static void SaveToPath(const std::string& path, const DataType& data)
        {
            impl::SaveToPath(path, data);
        }
    };

    using BufferAssetPtr = std::shared_ptr<Asset<Buffer>>;
}