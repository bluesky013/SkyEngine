//
// Created by Zach Lee on 2025/10/12.
//

#pragma once

#include <core/shapes/TriangleMesh.h>
#include <core/archive/BinaryData.h>
#include <core/memory/Allocator.h>

namespace sky {

    class MeshVertexDataInterface {
    public:
        MeshVertexDataInterface() = default;
        virtual ~MeshVertexDataInterface() = default;

        virtual void Resize(uint32_t size) = 0;

        virtual uint32_t GetStride() const = 0;

        virtual uint32_t Num() const = 0;

        virtual uint8_t* GetDataPointer() = 0;

        template <typename T>
        void SetVertexData(uint32_t index, const T& val, uint32_t offset = 0)
        {
            uint8_t* ptr = GetDataPointer() + index * GetStride();
            memcpy(ptr + offset, &val, sizeof(T));
        }

        template <typename T>
        const T& GetVertexData(uint32_t index, uint32_t offset = 0)
        {
            const uint8_t* ptr = GetDataPointer() + index * GetStride();
            const T* data = reinterpret_cast<const T*>(ptr + offset);
            return *data;
        }
    };

    template <typename T>
    class TRawMeshVertexData : public MeshVertexDataInterface {
    public:
        TRawMeshVertexData() = default;

        TRawMeshVertexData(size_t vertexNum)
        {
            numVertex = static_cast<uint32_t>(vertexNum);
            rawData = new BinaryData(numVertex * sizeof(T));
        }

        ~TRawMeshVertexData() override = default;

        void Resize(uint32_t newNum) override
        {
            auto *data = new BinaryData(newNum * sizeof(T));

            if (rawData) {
                memcpy(data->Data(), rawData->Data(), numVertex * sizeof(T));
            }

            rawData.Reset(data);
            numVertex = newNum;
        }

        uint32_t GetStride() const override
        {
            return static_cast<uint32_t>(sizeof(T));
        }

        uint32_t Num() const override
        {
            return numVertex;
        }

        uint8_t* GetDataPointer() override
        {
            return rawData ? rawData->Data() : nullptr;
        }

    private:
        uint32_t numVertex = 0;
        BinaryDataPtr rawData;
    };

    class RawMeshIndexData {
    public:
        explicit RawMeshIndexData(rhi::IndexType type) : indexType(type) {}

        explicit RawMeshIndexData(size_t indexNum, rhi::IndexType type) : indexType(type)
        {
            numIndex = static_cast<uint32_t>(indexNum);
            rawData = new BinaryData(numIndex * GetIndexSize());
        }

        ~RawMeshIndexData() = default;

        void Resize(uint32_t newNum)
        {
            auto *data = new BinaryData(newNum * GetIndexSize());

            if (rawData) {
                memcpy(data->Data(), rawData->Data(), numIndex * GetIndexSize());
            }

            rawData.Reset(data);
            numIndex = newNum;
        }

        uint32_t Num() const
        {
            return numIndex;
        }

        uint8_t* GetDataPointer()
        {
            return rawData ? rawData->Data() : nullptr;
        }

        void SetIndex(uint32_t idx, uint32_t val)
        {
            uint8_t* ptr = rawData->Data() + idx * GetIndexSize();
            if (indexType == rhi::IndexType::U16) {
                (*reinterpret_cast<uint16_t*>(ptr)) = val;
            } else {
                (*reinterpret_cast<uint32_t*>(ptr)) = val;
            }
        }

        uint16_t GetIndexU16(uint32_t idx)
        {
            const uint8_t* ptr = rawData->Data() + idx * GetIndexSize();
            if (indexType == rhi::IndexType::U16) {
                return *reinterpret_cast<const uint16_t*>(ptr);
            } else {
                return static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>(ptr));
            }
        }

        uint32_t GetIndexU32(uint32_t idx)
        {
            const uint8_t* ptr = rawData->Data() + idx * GetIndexSize();
            if (indexType == rhi::IndexType::U16) {
                return *reinterpret_cast<const uint16_t*>(ptr);
            } else {
                return *reinterpret_cast<const uint32_t*>(ptr);
            }
        }

        inline rhi::IndexType GetIndexType() const
        {
            return indexType;
        }

    private:
        uint32_t GetIndexSize() const
        {
            return indexType == rhi::IndexType::U32 ? sizeof(uint32_t) : sizeof(uint16_t);
        }

        rhi::IndexType indexType = rhi::IndexType::U32;
        uint32_t numIndex = 0;
        BinaryDataPtr rawData;
    };

} // namespace sky