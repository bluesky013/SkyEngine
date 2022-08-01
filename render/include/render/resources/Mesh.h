//
// Created by Zach Lee on 2022/5/26.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/Material.h>
#include <render/resources/Buffer.h>
#include <core/math/Box.h>

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
        uint32_t index  = 0;
        uint32_t offset = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    enum class MeshAttribute : uint32_t {
        POSITION,
        NORMAL,
        TANGENT,
        COLOR,
        UV0
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
            Builder& AddVertexBuffer(const RDBufferViewPtr& buffer);
            Builder& AddVertexDesc(const VertexDesc& desc);
            Builder& AddSubMesh(const SubMesh& mesh);

        private:
            Mesh& mesh;
        };

        const std::vector<SubMesh>& GetSubMeshes() const;

        const std::vector<VertexDesc>& GetVertexDesc() const;

        const std::vector<RDBufferViewPtr>& GetVertexBuffers() const;

        RDBufferViewPtr GetIndexBuffer() const;

        VkIndexType GetIndexType() const;

    private:
        friend class Builder;
        RDBufferViewPtr indexBuffer;
        std::vector<RDBufferViewPtr> vertexBuffers;
        std::vector<VertexDesc> vertexDescriptions;
        std::vector<SubMesh> subMeshes;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    };

    using RDMeshPtr = std::shared_ptr<Mesh>;
}