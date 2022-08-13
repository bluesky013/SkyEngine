//
// Created by Zach Lee on 2022/5/26.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/Material.h>
#include <render/resources/Buffer.h>
#include <core/math/Box.h>
#include <vulkan/DrawItem.h>

namespace sky {

    struct SubMeshDrawData {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
    };

    struct SubMesh {
        SubMeshDrawData drawData;
        Box aabb;
        RDMaterialPtr material;
    };

    struct VertexDesc {
        std::string name;
        uint32_t index  = 0;
        uint32_t offset = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    enum class MeshAttributeType : uint8_t {
        POSITION = 0,
        NORMAL,
        TANGENT,
        COLOR,
        UV0,
        NUM,
    };

    struct MeshRawData {
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> tangents;
        std::vector<float> colors;
        std::vector<float> uvs;

        std::size_t Size() const
        {
            return (positions.size() + normals.size() + tangents.size() + colors.size() + uvs.size()) * sizeof(float);
        }
    };

    struct BufferAssetView {
        BufferAssetPtr buffer;
        uint64_t offset = 0;
        uint64_t size   = 0;
        uint32_t stride = 0;
    };

    struct SubMeshAsset {
        SubMeshDrawData drawData;
        Box aabb;
    };

    struct MeshAssetData {
        BufferAssetView indexBuffer;
        std::vector<BufferAssetView> vertexBuffers;
        std::vector<VertexDesc> vertexDescriptions;
        std::vector<SubMeshAsset> subMeshes;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    };

    class Mesh : public RenderResource {
    public:
        Mesh() = default;
        ~Mesh() = default;

        class Builder {
        public:
            Builder(Mesh& m);
            ~Builder() = default;

            Builder& SetIndexBuffer(const RDBufferViewPtr& buffer, VkIndexType type = VK_INDEX_TYPE_UINT32);
            Builder& AddVertexBuffer(const RDBufferViewPtr& buffer, VkVertexInputRate rate = VK_VERTEX_INPUT_RATE_VERTEX);
            Builder& AddVertexDesc(const VertexDesc& desc);
            Builder& AddSubMesh(const SubMesh& mesh);

        private:
            Mesh& mesh;
        };

        const SubMesh& GetSubMesh(uint32_t index) const;

        const std::vector<SubMesh>& GetSubMeshes() const;

        const std::vector<VertexDesc>& GetVertexDesc() const;

        const std::vector<RDBufferViewPtr>& GetVertexBuffers() const;

        RDBufferViewPtr GetIndexBuffer() const;

        VkIndexType GetIndexType() const;

        drv::VertexInputPtr BuildVertexInput(Shader& vertexShader) const;

        drv::CmdDraw BuildDrawArgs(uint32_t subMesh) const;

    private:
        friend class Builder;
        RDBufferViewPtr indexBuffer;
        std::vector<RDBufferViewPtr> vertexBuffers;
        std::vector<VkVertexInputRate> inputRates;
        std::vector<VertexDesc> vertexDescriptions;
        std::vector<SubMesh> subMeshes;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    };

    using RDMeshPtr = std::shared_ptr<Mesh>;

    namespace impl {
        void LoadFromPath(const std::string& path, MeshAssetData& data);
        void SaveToPath(const std::string& path, const MeshAssetData& data);
        Mesh* CreateFromData(const MeshAssetData& data);
    }

    template <>
    struct AssetTraits <Mesh> {
        using DataType = MeshAssetData;

        static void LoadFromPath(const std::string& path, DataType& data)
        {
            impl::LoadFromPath(path, data);
        }

        static Mesh* CreateFromData(const DataType& data)
        {
            return impl::CreateFromData(data);
        }

        static void SaveToPath(const std::string& path, const DataType& data)
        {
            impl::SaveToPath(path, data);
        }
    };

    using MeshAssetPtr = std::shared_ptr<Asset<Mesh>>;
}