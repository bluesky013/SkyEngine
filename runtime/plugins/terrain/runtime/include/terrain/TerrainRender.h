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
    class RenderScene;

    class TerrainRender {
    public:
        explicit TerrainRender(const TerrainQuad& quad);
        ~TerrainRender() = default;

        void Tick();

        void SetMaterial(const RDMaterialInstancePtr &mat);
        void AddSector(const TerrainSector &sector);
        void RemoveSector(const TerrainCoord &coord);
        bool HasSector(const TerrainCoord &coord) const;
        void BuildSectors();

        void DetachFromScene(RenderScene* scene);
        void AttachToScene(RenderScene* scene);
    private:
        void BuildGeometry();

        static void BuildVertexBuffer(const TerrainQuad &quad, VertexBuffer &vertexBuffer);
        static void BuildIndexBufferLod(const TerrainQuad &quad, IndexBuffer &indexBuffer, uint32_t lod);

        TerrainQuad config;

        VertexBuffer             vertexBuffer;
        std::vector<IndexBuffer> indexBuffers;

        VertexBuffer             instanceBuffer;
        RDBufferPtr              instanceDataBuffer;

        RDMaterialInstancePtr    material;
        RDResourceGroupPtr       batchSet;

        std::vector<TerrainSector> sectors;
        std::vector<std::unique_ptr<TerrainSectorRender>> sectorRenders;
    };

} // namespace sky
