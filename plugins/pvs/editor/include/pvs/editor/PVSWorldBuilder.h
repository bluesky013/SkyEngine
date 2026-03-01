//
// Created by Zach Lee on 2026/2/24.
//

#pragma once

#include <framework/interface/IWorldBuilder.h>
#include <pvs/PVSLoader.h>
#include <pvs/PVSVisualizer.h>
#include <core/util/BitUtil.h>

namespace sky {
    class RenderScene;
} // namespace sky

namespace sky::editor {

    struct PVSSampleParam {
        Vector3 localPositionInCell;
        Vector3 direction;
    };

    struct PVSSectorBuilder {
        PVSSectorBuilder(const PVSConfig &config, uint32_t cellDataSize)
        {
            sector = std::make_shared<PVSSector>();
            sector->Init(config, cellDataSize);
        }

        /**
         * @brief Allocate a cell in this sector and return a pointer to its
         *        visibility data within the chunk storage.
         *
         * @param config        PVS grid configuration
         * @param cellDataSize  bytes of visibility data per cell
         * @param pos           world-space position inside the cell
         * @return pointer to the cell's visibility data, or nullptr on failure
         */
        uint8_t* Allocate(const PVSConfig &config, uint32_t cellDataSize, const Vector3 &pos)
        {
            auto index = config.CalculateCellIndexInSector(pos);
            if (index >= sector->cells.size()) {
                return nullptr;
            }

            auto &cell = sector->cells[index];

            // Already allocated — return existing data pointer
            if (cell.chunkIndex != static_cast<uint16_t>(~0)) {
                return sector->chunks[cell.chunkIndex].storage.get() + cell.dataOffset;
            }

            // Need a new chunk?
            if (sector->chunks.empty() || currentOffset >= config.cellsPerChunk) {
                PVSChunk chunk;
                uint32_t chunkBytes = cellDataSize * config.cellsPerChunk;
                chunk.storage = std::make_unique<uint8_t[]>(chunkBytes);
                std::memset(chunk.storage.get(), 0, chunkBytes);

                sector->chunks.emplace_back(std::move(chunk));
                currentChunk = static_cast<uint32_t>(sector->chunks.size() - 1);
                currentOffset = 0;
            }

            cell.chunkIndex = static_cast<uint16_t>(currentChunk);
            cell.dataOffset = static_cast<uint16_t>(currentOffset * cellDataSize);
            ++currentOffset;

            return sector->chunks[cell.chunkIndex].storage.get() + cell.dataOffset;
        }

        uint32_t currentChunk = 0;
        uint32_t currentOffset = 0;

        std::shared_ptr<PVSSector> sector;
    };

    struct PVSBuildContext {
        /**
         * @brief Allocate visibility storage for the cell at the given world position.
         *
         * Finds (or creates) the sector builder for the position's sector,
         * allocates a chunk slot for the cell, and returns a pointer to the
         * cell's zeroed visibility bitset within the chunk.
         *
         * @return pointer to cellDataSize bytes of visibility data, or nullptr on failure
         */
        uint8_t* GetOrAllocateCellData(const Vector3 &pos)
        {
            auto sectorCoord = config.CalculateSectorCoordByWorldPosition(pos);
            auto iter = sectors.find(sectorCoord);
            if (iter == sectors.end()) {
                iter = sectors.emplace(sectorCoord, PVSSectorBuilder(config, cellDataSize)).first;
            }
            return iter->second.Allocate(config, cellDataSize, pos);
        }

        void Save();

        PVSConfig                    config;
        RenderScene                 *scene = nullptr;
        uint32_t                     cellDataSize = 0;

        std::vector<Vector3>         sortedCells;
        std::vector<PVSSampleParam>  samples;

        std::vector<PVSDrawGeometryInstance> objectGeometryInstance;

        FilePath                     savePath;
        std::unordered_map<PVSSectorCoord, PVSSectorBuilder, PVSSectorCoordHash> sectors;
    };

    struct CellBuildTask {
        uint32_t cellID;
    };

    class PVSWorldBuilder : public IWorldBuilder {
    public:
        PVSWorldBuilder();
        ~PVSWorldBuilder() override = default;

        void Build(const WorldPtr &) const override;

        void BuildSamples(std::vector<PVSSampleParam> &samples) const;

        std::string GetDesc() const override
        {
            return "Build Precompute Visibility";
        }

    private:
        PVSConfig config;
    };

} // namespace sky::editor