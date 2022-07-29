//
// Created by Zach Lee on 2022/5/26.
//


#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/Material.h>
#include <render/resources/Buffer.h>
#include <core/math/Box.h>

namespace sky {

    struct SubMesh {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;

        Box aabb;
        RDMaterialPtr material;
    };

    struct VertexDesc {
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
            Builder();
            ~Builder() = default;

            Builder& SetIndexBuffer(const BufferView& buffer);
            Builder& AddVertexBuffer(const BufferView& buffer);
            Builder& AddVertexDesc(const VertexDesc& desc);
            Builder& AddSubMesh(const SubMesh& mesh);
            std::shared_ptr<Mesh> Build();

        private:
            std::shared_ptr<Mesh> mesh;
        };

    private:
        friend class Builder;
        BufferView indexBuffer;
        std::vector<BufferView> vertexBuffers;
        std::vector<VertexDesc> vertexDescriptions;
        std::vector<SubMesh> subMeshes;
    };

    using RDMeshPtr = std::shared_ptr<Mesh>;
}