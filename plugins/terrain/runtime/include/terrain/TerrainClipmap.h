//
// Created on 2025/01/15.
//

#pragma once

#include <terrain/TerrainBase.h>
#include <render/resource/Buffer.h>
#include <render/RenderGeometry.h>
#include <core/math/Vector3.h>
#include <vector>

namespace sky {

    // Per-block instance data uploaded to GPU each frame
    struct ClipmapBlockGPU {
        float offsetX;  // world X origin of this block
        float offsetZ;  // world Z origin of this block
        float scale;    // vertex spacing = resolution * 2^level
        uint32_t level; // clipmap level index
    };

    // Per-level bookkeeping
    struct ClipmapLevelInfo {
        float   snapX  = 0.f;
        float   snapZ  = 0.f;
        float   scale  = 1.f; // resolution * 2^level
        uint32_t blockCount = 0;
    };

    class TerrainClipmap {
    public:
        TerrainClipmap() = default;
        ~TerrainClipmap() = default;

        void Init(const ClipmapConfig &config);

        // Recompute block positions based on camera position
        void UpdateSnapPositions(const Vector3 &cameraPos);

        // Access shared geometry (one VB/IB for all blocks)
        const VertexBuffer &GetVertexBuffer() const { return vertexBuffer; }
        const IndexBuffer  &GetIndexBuffer()  const { return indexBuffer; }

        uint32_t GetBlockVertexCount() const;
        uint32_t GetBlockIndexCount() const;

        const std::vector<ClipmapLevelInfo>  &GetLevels() const { return levels; }
        const std::vector<ClipmapBlockGPU>   &GetVisibleBlocks() const { return visibleBlocks; }
        const std::vector<ClipmapBlockGPU>   &GetBlocksForLevel(uint32_t level) const;

        uint32_t GetNumLevels() const { return numLevels; }

    private:
        void BuildSharedMesh();
        void ComputeRingBlocks(uint32_t level, float snapX, float snapZ);

        ClipmapConfig config = {};
        uint32_t blockSize  = 64;
        uint32_t numLevels  = 8;
        float    resolution = 1.0f;

        // Shared mesh (single block = blockSize × blockSize grid)
        VertexBuffer vertexBuffer;
        IndexBuffer  indexBuffer;

        // Per-level info
        std::vector<ClipmapLevelInfo> levels;

        // All visible blocks this frame
        std::vector<ClipmapBlockGPU> visibleBlocks;

        // Per-level block lists (for per-level draw calls)
        std::vector<std::vector<ClipmapBlockGPU>> perLevelBlocks;

        static constexpr uint32_t BLOCKS_PER_SIDE = 4;
    };

} // namespace sky
