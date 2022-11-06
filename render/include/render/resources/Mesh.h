//
// Created by Zach Lee on 2022/5/26.
//

#pragma once

#include <core/math/Box.h>
#include <render/resources/Buffer.h>
#include <render/resources/Material.h>
#include <render/resources/RenderResource.h>
#include <vulkan/DrawItem.h>

namespace sky {

    struct SubMeshDrawData {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex  = 0;
        uint32_t indexCount  = 0;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(firstVertex, vertexCount, firstIndex, indexCount);
        }
    };

    struct SubMesh {
        SubMeshDrawData drawData;
        Box             aabb;
        RDMaterialPtr   material;
    };

    struct VertexDesc {
        std::string name;
        uint32_t    index  = 0;
        uint32_t    offset = 0;
        VkFormat    format = VK_FORMAT_UNDEFINED;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(name, index, offset, format);
        }
    };

    enum class MeshAttributeType : uint8_t {
        POSITION = 0,
        NORMAL,
        TANGENT,
        BITANGENT,
        COLOR,
        UV0,
        NUM,
    };

    struct MeshRawData {
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> tangents;
        std::vector<float> biTangents;
        std::vector<float> colors;
        std::vector<float> uvs;

        std::size_t Size() const
        {
            return (positions.size() + normals.size() + tangents.size() + biTangents.size() + colors.size() + uvs.size()) * sizeof(float);
        }
    };

    struct BufferAssetView {
        BufferAssetPtr buffer;
        uint64_t       offset = 0;
        uint64_t       size   = 0;
        uint32_t       stride = 0;

        template <class Archive>
        void save(Archive &ar) const
        {
            ar(buffer->GetUuid(), offset, size, stride);
        }

        template <class Archive>
        void load(Archive &ar)
        {
            Uuid uuid;
            ar(uuid, offset, size, stride);
            InitBuffer(uuid);
        }

        void InitBuffer(const Uuid &id);

        RDBufferViewPtr CreateBufferView() const;
    };

    struct SubMeshAsset {
        SubMeshDrawData  drawData;
        MaterialAssetPtr material;
        Box              aabb;

        template <class Archive>
        void save(Archive &ar) const
        {
            Uuid uuid;
            ar(drawData, material->GetUuid(), aabb);
        }

        template <class Archive>
        void load(Archive &ar)
        {
            Uuid uuid;
            ar(drawData, uuid, aabb);
            InitMaterial(uuid);
        }

        void InitMaterial(const Uuid &id);
        SubMesh ToSubMesh() const;
    };

    struct MeshAssetData {
        BufferAssetView              indexBuffer;
        std::vector<BufferAssetView> vertexBuffers;
        std::vector<VertexDesc>      vertexDescriptions;
        std::vector<SubMeshAsset>    subMeshes;
        VkIndexType                  indexType = VK_INDEX_TYPE_UINT32;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(indexBuffer, vertexBuffers, vertexDescriptions, subMeshes, indexType);
        }
    };

    class Mesh : public RenderResource {
    public:
        Mesh()  = default;
        ~Mesh() = default;

        class Builder {
        public:
            Builder(Mesh &m);
            ~Builder() = default;

            Builder &SetIndexBuffer(const RDBufferViewPtr &buffer, VkIndexType type = VK_INDEX_TYPE_UINT32);
            Builder &AddVertexBuffer(const RDBufferViewPtr &buffer, VkVertexInputRate rate = VK_VERTEX_INPUT_RATE_VERTEX);
            Builder &AddVertexDesc(const VertexDesc &desc);
            Builder &SetVertexDesc(const std::vector<VertexDesc> &desc);
            Builder &AddSubMesh(const SubMesh &mesh);

        private:
            Mesh &mesh;
        };

        const SubMesh &GetSubMesh(uint32_t index) const;

        const std::vector<SubMesh> &GetSubMeshes() const;

        const std::vector<VertexDesc> &GetVertexDesc() const;

        const std::vector<RDBufferViewPtr> &GetVertexBuffers() const;

        RDBufferViewPtr GetIndexBuffer() const;

        VkIndexType GetIndexType() const;

        vk::VertexInputPtr BuildVertexInput(Shader &vertexShader) const;

        vk::CmdDraw BuildDrawArgs(uint32_t subMesh) const;

        uint32_t GetSubMeshCount() const;

        void SetMaterial(RDMaterialPtr material, uint32_t index);

        static std::shared_ptr<Mesh> CreateFromData(const MeshAssetData &data);

    private:
        friend class Builder;
        RDBufferViewPtr                indexBuffer;
        std::vector<RDBufferViewPtr>   vertexBuffers;
        std::vector<VkVertexInputRate> inputRates;
        std::vector<VertexDesc>        vertexDescriptions;
        std::vector<SubMesh>           subMeshes;
        VkIndexType                    indexType = VK_INDEX_TYPE_UINT32;
    };

    using RDMeshPtr = std::shared_ptr<Mesh>;

    template <>
    struct AssetTraits<Mesh> {
        using DataType                                = MeshAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("394AB7FF-FC10-484F-82A6-42D523949DD1");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static RDMeshPtr CreateFromData(const DataType &data)
        {
            return Mesh::CreateFromData(data);
        }
    };
    using MeshAssetPtr = std::shared_ptr<Asset<Mesh>>;
} // namespace sky
