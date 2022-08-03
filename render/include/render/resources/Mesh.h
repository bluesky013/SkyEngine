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
}