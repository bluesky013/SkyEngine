//
// Created by blues on 2025/5/5.
//

#pragma once

#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <render/RenderBase.h>
#include <render/RenderGeometry.h>
#include <core/template/ReferenceObject.h>
#include <array>
#include <string_view>
#include <utility>

namespace sky {

    struct VFPos3F {
        Vector3 pos;

        static constexpr uint32_t ATTRIBUTE_NUM = 1;
        static constexpr std::string_view SEMANTICS[] = {"POSITION"};
        static constexpr uint32_t OFFSETS[] = {0};
        static constexpr rhi::Format VFM[] = {rhi::Format::F_RGB32};
        static constexpr VertexSemanticFlagBit SEMANTIC_FLAGS[] = { VertexSemanticFlagBit::POSITION};
        static constexpr VertexSemanticFlags flags = VertexSemanticFlagBit::POSITION;
    };

    struct VFPos3F_UV2F {
        Vector3 pos;
        Vector2 uv;

        static constexpr uint32_t ATTRIBUTE_NUM = 2;
        static constexpr std::string_view SEMANTICS[] = {"POSITION", "UV"};
        static constexpr uint32_t OFFSETS[] = {0, 12};
        static constexpr rhi::Format VFM[] = {rhi::Format::F_RGB32, rhi::Format::F_RG32};
        static constexpr VertexSemanticFlagBit SEMANTIC_FLAGS[] = { VertexSemanticFlagBit::POSITION, VertexSemanticFlagBit::UV };
        static constexpr VertexSemanticFlags flags = VertexSemanticFlagBit::POSITION | VertexSemanticFlagBit::UV;
    };

    struct VFShading_NT {
        Vector4 normal;
        Vector4 tangent;

        static constexpr uint32_t ATTRIBUTE_NUM = 2;
        static constexpr std::string_view SEMANTICS[] = {"NORMAL", "TANGENT"};
        static constexpr uint32_t OFFSETS[] = {0, 16};
        static constexpr rhi::Format VFM[] = {rhi::Format::F_RGBA32, rhi::Format::F_RGBA32};
        static constexpr VertexSemanticFlagBit SEMANTIC_FLAGS[] = { VertexSemanticFlagBit::NORMAL, VertexSemanticFlagBit::TANGENT };
        static constexpr VertexSemanticFlags flags = VertexSemanticFlagBit::TANGENT | VertexSemanticFlagBit::NORMAL;
    };

    class VFDataStorage : public RefObject {
    public:
        VFDataStorage() = default;
        ~VFDataStorage() override = default;

        virtual const uint8_t *GetDataConst() const = 0;
        virtual uint8_t *GetData() = 0;
        virtual uint32_t GetSize() const = 0;
    };

    class VFRawDataStorage : public VFDataStorage {
    public:
        explicit VFRawDataStorage(const uint8_t* ptr, uint32_t sz) : data(ptr), size(sz) {}
        ~VFRawDataStorage() override = default;

        const uint8_t *GetDataConst() const override { return data; }
        uint8_t *GetData() override { return nullptr; }
        uint32_t GetSize() const override { return size; }
    private:
        const uint8_t *data = nullptr;
        uint32_t size = 0;
    };

    class VFVectorStorage : public VFDataStorage {
    public:
        VFVectorStorage() = default;
        ~VFVectorStorage() override = default;

        const uint8_t *GetDataConst() const override { return data.data(); }
        uint8_t *GetData() override { return data.data(); }
        uint32_t GetSize() const override { return (uint32_t)data.size(); }
    private:
        std::vector<uint8_t> data;
    };
    using VFDataStoragePtr = CounterPtr<VFDataStorage>;

    template <typename T>
    class VFDataViewer {
    public:
        explicit VFDataViewer(VFDataStoragePtr storage) : data(std::move(storage))
        {
        }

        const uint8_t *GetDataConst() const { return data->GetDataConst(); }
        uint8_t *GetData() { return data->GetData(); }
        uint32_t GetVertexNum() const { return data->GetSize() / sizeof(T); }
    private:
        VFDataStoragePtr data;
    };

    class BaseGeometry : public RefObject {
    public:
        BaseGeometry() = default;
        ~BaseGeometry() override = default;
    };

    template <typename ...Stream>
    class TGeometry : public BaseGeometry {
    public:
        static constexpr uint32_t STREAM_COUNT = sizeof...(Stream);
        static constexpr uint32_t ATTRIBUTE_COUNT = (0 + ... + Stream::ATTRIBUTE_NUM);
        using Type = std::tuple<Stream...>;

        template<size_t... I>
        constexpr size_t SUM(std::integer_sequence<size_t, I...>)
        {
            return (0 + ... + (std::tuple_element_t<I, Type>::ATTRIBUTE_NUM));
        }

        template <size_t B, size_t A>
        constexpr void SetAttribute()
        {
            using StreamType = std::tuple_element_t<B, Type>;

            size_t base = SUM(std::make_integer_sequence<size_t, B>{}) + A;;
            attributes[base].sematic = StreamType::SEMANTIC_FLAGS[A];
            attributes[base].binding = (uint32_t)B;
            attributes[base].offset  = StreamType::OFFSETS[A];
            attributes[base].format = StreamType::VFM[A];
        }
        template <size_t I>
        constexpr void SetStream()
        {
            using StreamType = std::tuple_element_t<I, Type>;

            streams[I].stride = sizeof(StreamType);

            [this]<size_t... J>(std::index_sequence<J...>) {
                (SetAttribute<I, J>(), ...);
            }(std::make_index_sequence<StreamType::ATTRIBUTE_NUM>{});
        }

        constexpr TGeometry()
        {
            [this]<size_t... I>(std::index_sequence<I...>) {
                (SetStream<I>(), ...);
            }(std::make_index_sequence<sizeof...(Stream)>{});
        }

        template <size_t I>
        VFDataViewer<std::tuple_element_t<I, Type>> CreateInterface()
        {
            return VFDataViewer<std::tuple_element_t<I, Type>>(datas[I]);
        }

        void SetVertexData(uint32_t stream, const VFDataStoragePtr &storage)
        {
            datas[stream] = storage;
        }

        void SetIndices(const VFDataStoragePtr& storage, rhi::IndexType type)
        {
            indices = storage;
            indexType = type;
        }

    private:
        std::array<VertexAttribute, ATTRIBUTE_COUNT> attributes;
        std::array<VertexStream, STREAM_COUNT> streams;
        std::array<VFDataStoragePtr, STREAM_COUNT> datas;

        VFDataStoragePtr indices;
        rhi::IndexType indexType = rhi::IndexType::U32;
    };

    using GeometryPtr = CounterPtr<BaseGeometry>;

    enum class BuiltinGeometryType : uint32_t {
        CUBE
    };

    class GeometryFactory {
    public:
        GeometryFactory() = default;
        ~GeometryFactory() = default;

        GeometryPtr CreateGeometry(BuiltinGeometryType type);

    private:
        std::unordered_map<BuiltinGeometryType, GeometryPtr> geometries;
    };

} // namespace sky
