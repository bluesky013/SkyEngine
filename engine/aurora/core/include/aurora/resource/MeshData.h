//
// MeshData: CPU-side mesh data interface. Ports the old render/resource/
// MeshInterface.h onto the aurora resource layer; vertex and index data are
// carried in core/archive/BinaryData (RefObject byte buffer).
//

#pragma once

#include <aurora/rhi/Core.h>
#include <core/archive/BinaryData.h>

#include <cstdint>
#include <cstring>

namespace sky::aurora {

    // Type-agnostic vertex data stream. The vertex struct T is decoupled from
    // the underlying byte buffer, so static/skinned meshes and in-memory /
    // file-loaded sources share one build + upload path.
    class MeshVertexDataInterface {
    public:
        virtual ~MeshVertexDataInterface() = default;

        virtual void     Resize(uint32_t count) = 0;
        virtual uint32_t GetStride() const = 0;
        virtual uint32_t Count() const = 0;
        virtual uint8_t       *GetData() = 0;
        virtual const uint8_t *GetData() const = 0;

        template <typename T>
        void SetVertexData(uint32_t index, const T &val, uint32_t offset = 0)
        {
            uint8_t *ptr = GetData() + static_cast<size_t>(index) * GetStride();
            std::memcpy(ptr + offset, &val, sizeof(T));
        }

        template <typename T>
        const T &GetVertexData(uint32_t index, uint32_t offset = 0) const
        {
            const uint8_t *ptr = GetData() + static_cast<size_t>(index) * GetStride();
            return *reinterpret_cast<const T *>(ptr + offset);
        }
    };

    template <typename T>
    class TRawMeshVertexData : public MeshVertexDataInterface {
    public:
        TRawMeshVertexData() = default;
        explicit TRawMeshVertexData(uint32_t count)
        {
            Resize(count);
        }

        void Resize(uint32_t count) override
        {
            auto *next = new BinaryData(count * sizeof(T));
            if (data != nullptr) {
                std::memcpy(next->Data(), data->Data(), static_cast<size_t>(numVertex) * sizeof(T));
            }
            data.Reset(next);
            numVertex = count;
        }

        uint32_t GetStride() const override { return static_cast<uint32_t>(sizeof(T)); }
        uint32_t Count() const override { return numVertex; }

        uint8_t *GetData() override { return data != nullptr ? data->Data() : nullptr; }
        const uint8_t *GetData() const override { return data != nullptr ? data->Data() : nullptr; }

    private:
        uint32_t      numVertex = 0;
        BinaryDataPtr data;
    };

    // U16/U32 index data. SetIndex on U16 writes the low 16 bits; the caller
    // must keep values within 0xFFFF.
    class RawMeshIndexData {
    public:
        explicit RawMeshIndexData(IndexType type) : indexType(type) {}
        RawMeshIndexData(uint32_t count, IndexType type) : indexType(type)
        {
            Resize(count);
        }

        void Resize(uint32_t count)
        {
            auto *next = new BinaryData(count * GetIndexSize());
            if (data != nullptr) {
                std::memcpy(next->Data(), data->Data(), static_cast<size_t>(numIndex) * GetIndexSize());
            }
            data.Reset(next);
            numIndex = count;
        }

        uint32_t Count() const { return numIndex; }
        uint8_t *GetData() { return data != nullptr ? data->Data() : nullptr; }

        void SetIndex(uint32_t idx, uint32_t val)
        {
            if (data == nullptr || idx >= numIndex) {
                return;
            }
            uint8_t *ptr = data->Data() + static_cast<size_t>(idx) * GetIndexSize();
            if (indexType == IndexType::U16) {
                *reinterpret_cast<uint16_t *>(ptr) = static_cast<uint16_t>(val);
            } else {
                *reinterpret_cast<uint32_t *>(ptr) = val;
            }
        }

        uint32_t GetIndex(uint32_t idx) const
        {
            if (data == nullptr || idx >= numIndex) {
                return 0;
            }
            const uint8_t *ptr = data->Data() + static_cast<size_t>(idx) * GetIndexSize();
            if (indexType == IndexType::U16) {
                return *reinterpret_cast<const uint16_t *>(ptr);
            }
            return *reinterpret_cast<const uint32_t *>(ptr);
        }

        IndexType GetIndexType() const { return indexType; }

    private:
        uint32_t GetIndexSize() const
        {
            return indexType == IndexType::U32 ? sizeof(uint32_t) : sizeof(uint16_t);
        }

        IndexType     indexType = IndexType::U32;
        uint32_t      numIndex  = 0;
        BinaryDataPtr data;
    };

} // namespace sky::aurora
