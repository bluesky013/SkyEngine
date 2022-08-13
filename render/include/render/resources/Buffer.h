//
// Created by Zach Lee on 2022/5/24.
//


#pragma once

#include <framework/asset/Asset.h>
#include <render/resources/RenderResource.h>
#include <vulkan/Buffer.h>
#include <vector>

namespace sky {

    struct BufferAssetData {
        std::vector<uint8_t> data;
    };

    class Buffer : public RenderResource {
    public:
        struct Descriptor {
            VkDeviceSize        size     = 0;
            VkBufferUsageFlags  usage    = 0;
            VmaMemoryUsage      memory   = VMA_MEMORY_USAGE_CPU_TO_GPU;
            bool                allocCPU = false;
        };

        Buffer() = default;
        Buffer(const Descriptor& desc);
        ~Buffer() = default;

        void Init(const Descriptor& desc);

        void InitRHI();

        bool IsValid() const override;

        void Write(const uint8_t* data, uint64_t size, uint64_t offset = 0);

        template <typename T>
        void Write(const T& value, uint64_t offset)
        {
            Write(reinterpret_cast<const uint8_t*>(&value), sizeof(T), offset);
        }

        void Update(const uint8_t* data, uint64_t size);

        void Update(bool release = false);

        drv::BufferPtr GetRHIBuffer() const;

    protected:
        Descriptor descriptor;
        std::vector<uint8_t> rawData;
        drv::BufferPtr rhiBuffer;
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

        void RequestUpdate();

        template <typename T>
        void Write(const T& value, uint64_t off = 0)
        {
            if (buffer) {
                buffer->Write(value, offset + off);
            }
        }

    private:
        RDBufferPtr buffer;
        VkDeviceSize size = 0;
        VkDeviceSize offset = 0;
        uint32_t stride = 0;
    };
    using RDBufferViewPtr = std::shared_ptr<BufferView>;

    namespace impl {
        void LoadFromPath(const std::string& path, BufferAssetData& data);
        void SaveToPath(const std::string& path, const BufferAssetData& data);
        Buffer* CreateFromData(const BufferAssetData& data);
    }

    template <>
    struct AssetTraits <Buffer> {
        using DataType = BufferAssetData;

        static void LoadFromPath(const std::string& path, DataType& data)
        {
            impl::LoadFromPath(path, data);
        }

        static Buffer* CreateFromData(const DataType& data)
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