//
// Created on 2025/01/15.
//

#include <terrain/TerrainClipmap.h>
#include <cmath>
#include <cstring>

namespace sky {

    void TerrainClipmap::Init(const ClipmapConfig &cfg)
    {
        config     = cfg;
        blockSize  = cfg.blockSize;
        numLevels  = cfg.numLevels;
        resolution = cfg.resolution;

        levels.resize(numLevels);
        perLevelBlocks.resize(numLevels);

        for (uint32_t L = 0; L < numLevels; ++L) {
            levels[L].scale = resolution * static_cast<float>(1u << L);
        }

        BuildSharedMesh();
    }

    void TerrainClipmap::BuildSharedMesh()
    {
        uint32_t vertsPerSide = blockSize + 1;
        uint32_t vertCount    = vertsPerSide * vertsPerSide;
        uint32_t quads        = blockSize * blockSize;
        uint32_t idxCount     = quads * 6;

        // --- Vertex buffer: uint8 (x, y) pairs ---
        uint32_t vbSize = vertCount * sizeof(TerrainVertex);
        vertexBuffer.buffer = new Buffer();
        vertexBuffer.buffer->Init(vbSize, rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::CPU_TO_GPU);
        vertexBuffer.offset = 0;
        vertexBuffer.range  = vbSize;
        vertexBuffer.stride = sizeof(TerrainVertex);

        auto *vPtr = reinterpret_cast<TerrainVertex*>(vertexBuffer.buffer->GetRHIBuffer()->Map());
        for (uint32_t y = 0; y < vertsPerSide; ++y) {
            for (uint32_t x = 0; x < vertsPerSide; ++x) {
                vPtr->x = static_cast<uint8_t>(x);
                vPtr->y = static_cast<uint8_t>(y);
                ++vPtr;
            }
        }
        vertexBuffer.buffer->GetRHIBuffer()->UnMap();

        // --- Index buffer (uint16 for blockSize <= 255) ---
        uint32_t ibSize = idxCount * sizeof(uint16_t);
        indexBuffer.buffer = new Buffer();
        indexBuffer.buffer->Init(ibSize, rhi::BufferUsageFlagBit::INDEX, rhi::MemoryType::CPU_TO_GPU);
        indexBuffer.offset    = 0;
        indexBuffer.range     = ibSize;
        indexBuffer.indexType  = rhi::IndexType::U16;

        auto *iPtr = reinterpret_cast<uint16_t*>(indexBuffer.buffer->GetRHIBuffer()->Map());
        for (uint32_t y = 0; y < blockSize; ++y) {
            for (uint32_t x = 0; x < blockSize; ++x) {
                auto i00 = static_cast<uint16_t>((x + 0) + (y + 0) * vertsPerSide);
                auto i10 = static_cast<uint16_t>((x + 1) + (y + 0) * vertsPerSide);
                auto i11 = static_cast<uint16_t>((x + 1) + (y + 1) * vertsPerSide);
                auto i01 = static_cast<uint16_t>((x + 0) + (y + 1) * vertsPerSide);

                *iPtr++ = i00;
                *iPtr++ = i10;
                *iPtr++ = i11;

                *iPtr++ = i11;
                *iPtr++ = i01;
                *iPtr++ = i00;
            }
        }
        indexBuffer.buffer->GetRHIBuffer()->UnMap();
    }

    void TerrainClipmap::UpdateSnapPositions(const Vector3 &cameraPos)
    {
        visibleBlocks.clear();
        for (auto &list : perLevelBlocks) {
            list.clear();
        }

        for (uint32_t L = 0; L < numLevels; ++L) {
            float scale         = levels[L].scale;
            float blockWorldSize = static_cast<float>(blockSize) * scale;

            // Snap center to integer multiples of blockWorldSize
            float snapX = std::floor(cameraPos.x / blockWorldSize) * blockWorldSize;
            float snapZ = std::floor(cameraPos.z / blockWorldSize) * blockWorldSize;

            levels[L].snapX = snapX;
            levels[L].snapZ = snapZ;

            ComputeRingBlocks(L, snapX, snapZ);
        }
    }

    void TerrainClipmap::ComputeRingBlocks(uint32_t level, float snapX, float snapZ)
    {
        float scale = levels[level].scale;
        float blockWorldSize = static_cast<float>(blockSize) * scale;

        // Half-extent in blocks from center
        int32_t halfBlocks = static_cast<int32_t>(BLOCKS_PER_SIDE) / 2;

        // Inner region covered by finer level (skip for level 0)
        float innerBlockWorldSize = 0.f;
        float innerSnapX = 0.f;
        float innerSnapZ = 0.f;
        int32_t innerHalf = 0;

        if (level > 0) {
            float finerScale = levels[level - 1].scale;
            innerBlockWorldSize = static_cast<float>(blockSize) * finerScale;
            innerSnapX = levels[level - 1].snapX;
            innerSnapZ = levels[level - 1].snapZ;
            // The finer level's footprint in our coordinate space
            // finer level also uses BLOCKS_PER_SIDE blocks, so its extent is:
            // BLOCKS_PER_SIDE * blockSize * finerScale = BLOCKS_PER_SIDE * blockWorldSize / 2
            innerHalf = static_cast<int32_t>(BLOCKS_PER_SIDE) / 2;
        }

        uint32_t count = 0;
        for (int32_t by = -halfBlocks; by < halfBlocks; ++by) {
            for (int32_t bx = -halfBlocks; bx < halfBlocks; ++bx) {
                float blockOriginX = snapX + static_cast<float>(bx) * blockWorldSize;
                float blockOriginZ = snapZ + static_cast<float>(by) * blockWorldSize;

                // For levels > 0, skip blocks that are covered by the finer level's footprint
                if (level > 0) {
                    float finerExtent = static_cast<float>(innerHalf) * innerBlockWorldSize;
                    float finerMinX = innerSnapX - finerExtent;
                    float finerMaxX = innerSnapX + finerExtent;
                    float finerMinZ = innerSnapZ - finerExtent;
                    float finerMaxZ = innerSnapZ + finerExtent;

                    float blockMinX = blockOriginX;
                    float blockMaxX = blockOriginX + blockWorldSize;
                    float blockMinZ = blockOriginZ;
                    float blockMaxZ = blockOriginZ + blockWorldSize;

                    // Skip if block is entirely inside the finer level's footprint
                    if (blockMinX >= finerMinX && blockMaxX <= finerMaxX &&
                        blockMinZ >= finerMinZ && blockMaxZ <= finerMaxZ) {
                        continue;
                    }
                }

                ClipmapBlockGPU block = {};
                block.offsetX = blockOriginX;
                block.offsetZ = blockOriginZ;
                block.scale   = scale;
                block.level   = level;

                visibleBlocks.push_back(block);
                perLevelBlocks[level].push_back(block);
                ++count;
            }
        }
        levels[level].blockCount = count;
    }

    uint32_t TerrainClipmap::GetBlockVertexCount() const
    {
        uint32_t s = blockSize + 1;
        return s * s;
    }

    uint32_t TerrainClipmap::GetBlockIndexCount() const
    {
        return blockSize * blockSize * 6;
    }

    const std::vector<ClipmapBlockGPU> &TerrainClipmap::GetBlocksForLevel(uint32_t level) const
    {
        return perLevelBlocks[level];
    }

} // namespace sky
