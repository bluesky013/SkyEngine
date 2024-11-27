//
// Created by blues on 2024/11/7.
//

#pragma once

#include <terrain/TerrainBase.h>
#include <terrain/TerrainQuadTree.h>
#include <terrain/TerrainSector.h>
#include <render/resource/Buffer.h>
#include <memory>

namespace sky {

    class TerrainRender {
    public:
        explicit TerrainRender(const TerrainQuad& quad);
        ~TerrainRender() = default;

        void Tick();

        void AddSector(const TerrainSector &sector);
        void RemoveSector(const TerrainCoord &coord);
        bool HasSector(const TerrainCoord &coord) const;
    private:
        void BuildGeometry();

        static void BuildVertexBuffer(const TerrainQuad &quad, VertexBuffer &vertexBuffer);
        static void BuildIndexBufferLod(const TerrainQuad &quad, IndexBuffer &indexBuffer, uint32_t lod);

        TerrainQuad config;

        VertexBuffer             vertexBuffer;
        std::vector<IndexBuffer> indexBuffers;

        std::vector<TerrainSector> sectors;
    };

} // namespace sky
