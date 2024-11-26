//
// Created by blues on 2024/11/7.
//

#pragma once

#include <terrain/TerrainBase.h>
#include <terrain/TerrainQuadTree.h>
#include <render/resource/Buffer.h>

namespace sky {

    class TerrainRender {
    public:
        explicit TerrainRender(const TerrainQuad& quad);
        ~TerrainRender() = default;

        void Tick();

    private:
        void BuildGeometry();

        static void BuildVertexBuffer(const TerrainQuad &quad, VertexBuffer &vertexBuffer);
        static void BuildIndexBufferLod(const TerrainQuad &quad, IndexBuffer &indexBuffer, uint32_t lod);

        TerrainQuad config;

        VertexBuffer vertexBuffer;
        std::vector<IndexBuffer> indexBuffers;
    };

} // namespace sky
