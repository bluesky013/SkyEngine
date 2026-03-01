//
// Created by Zach Lee on 2026/2/23.
//

#pragma once

#include <pvs/PVSTypes.h>
#include <core/util/ArrayBitFlag.h>
#include <core/math/Vector3.h>
#include <core/archive/StreamArchive.h>
#include <core/file/FileSystem.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/JsonArchive.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cmath>

namespace sky {

    /**
      * @brief Represents a single cell in the PVS grid
      */
    struct PVSCell {
        uint16_t chunkIndex = static_cast<uint16_t>(~0);
        uint16_t dataOffset = 0;
    };

    struct PVSChunk {
        std::unique_ptr<uint8_t[]> storage;
    };

     /**
       * @brief Configuration for PVS grid generation
       */
    struct PVSConfig {
        Vector3  worldOffset;
        float    cellSize;
        float    cellSizeY;
        int32_t  cellsInSectorXZ = 8; // number of cells along X and Z in each sector
        uint32_t cellsPerChunk = 16; // number of cells per chunk for visibility data storage

        void Load(BinaryInputArchive& archive)
        {
            archive.LoadValue(worldOffset);
            archive.LoadValue(cellSize);
            archive.LoadValue(cellSizeY);
            archive.LoadValue(cellsInSectorXZ);
            archive.LoadValue(cellsPerChunk);
        }

        void Save(BinaryOutputArchive& archive) const
        {
            archive.SaveValue(worldOffset);
            archive.SaveValue(cellSize);
            archive.SaveValue(cellSizeY);
            archive.SaveValue(cellsInSectorXZ);
            archive.SaveValue(cellsPerChunk);
        }

        /**
         * @brief Total number of cells per sector (XZ plane).
         */
        FORCEINLINE uint32_t GetCellsPerSector() const
        {
            return static_cast<uint32_t>(cellsInSectorXZ * cellsInSectorXZ);
        }

        /**
         * @brief World-space size of one sector along X or Z.
         */
        FORCEINLINE float GetSectorSize() const
        {
            return static_cast<float>(cellsInSectorXZ) * cellSize;
        }

        /**
          * @brief Calculate cell coordinate from world position (3D).
          */
        FORCEINLINE PVSCellCoord CalculateCellCoordByWorldPosition(const Vector3 &pos) const
        {
            PVSCellCoord coord;
            coord.x = static_cast<int32_t>(std::floor((pos.x - worldOffset.x) / cellSize));
            coord.y = static_cast<int32_t>(std::floor((pos.y - worldOffset.y) / cellSizeY));
            coord.z = static_cast<int32_t>(std::floor((pos.z - worldOffset.z) / cellSize));
            return coord;
        }

        /**
         * @brief Calculate sector coordinate from world position (2D XZ plane).
         */
        FORCEINLINE PVSSectorCoord CalculateSectorCoordByWorldPosition(const Vector3 &pos) const
        {
            const float sectorSize = GetSectorSize();
            PVSSectorCoord coord;
            coord.x = static_cast<int32_t>(std::floor((pos.x - worldOffset.x) / sectorSize));
            coord.y = static_cast<int32_t>(std::floor((pos.z - worldOffset.z) / sectorSize));
            return coord;
        }

        /**
         * @brief Calculate sector coordinate from a cell coordinate (2D XZ plane).
         */
        FORCEINLINE PVSSectorCoord CalculateSectorCoordByCellCoord(const PVSCellCoord &cellCoord) const
        {
            auto floorDiv = [](int32_t a, int32_t b) -> int32_t {
                return a / b - (a % b != 0 && (a ^ b) < 0 ? 1 : 0);
            };
            PVSSectorCoord coord;
            coord.x = floorDiv(cellCoord.x, cellsInSectorXZ);
            coord.y = floorDiv(cellCoord.z, cellsInSectorXZ);
            return coord;
        }

        /**
         * @brief Calculate the local cell index (flat XZ) within its sector.
         *        Returns the index into PVSSector::cells.
         */
        FORCEINLINE uint32_t CalculateCellIndexInSector(const Vector3 &pos) const
        {
            auto cellCoord = CalculateCellCoordByWorldPosition(pos);
            auto sectorCoord = CalculateSectorCoordByCellCoord(cellCoord);

            int32_t localX = cellCoord.x - (sectorCoord.x * cellsInSectorXZ);
            int32_t localZ = cellCoord.z - (sectorCoord.y * cellsInSectorXZ);

            return static_cast<uint32_t>(localZ * cellsInSectorXZ + localX);
        }

        /**
         * @brief Calculate the local cell index from a cell coordinate.
         */
        FORCEINLINE uint32_t CalculateCellIndexInSector(const PVSCellCoord &cellCoord) const
        {
            auto sectorCoord = CalculateSectorCoordByCellCoord(cellCoord);

            int32_t localX = cellCoord.x - (sectorCoord.x * cellsInSectorXZ);
            int32_t localZ = cellCoord.z - (sectorCoord.y * cellsInSectorXZ);

            return static_cast<uint32_t>(localZ * cellsInSectorXZ + localX);
        }

        /**
         * @brief Calculate the world-space min corner of a cell.
         */
        FORCEINLINE Vector3 CalculateCellWorldMin(const PVSCellCoord &cellCoord) const
        {
            return Vector3(
                static_cast<float>(cellCoord.x) * cellSize  + worldOffset.x,
                static_cast<float>(cellCoord.y) * cellSizeY + worldOffset.y,
                static_cast<float>(cellCoord.z) * cellSize  + worldOffset.z
            );
        }
    };

    struct PVSSector {
        void Init(const PVSConfig& config, uint32_t inCellDataSize)
        {
            cells.resize(config.cellsInSectorXZ * config.cellsInSectorXZ);
            chunkSize = inCellDataSize * config.cellsPerChunk;
        }

        void Load(BinaryInputArchive& bin)
        {
            bin.LoadValue(version);
            bin.LoadValue(chunkSize);

            uint32_t value = 0;
            bin.LoadValue(value);
            cells.resize(value);
            bin.LoadValue(reinterpret_cast<char *>(cells.data()), cells.size() * sizeof(PVSCell));

            bin.LoadValue(value);
            chunks.resize(value);

            for (uint32_t i = 0; i < value; ++i) {
                chunks[i].storage = std::make_unique<uint8_t[]>(chunkSize);
                bin.LoadValue(reinterpret_cast<char *>(chunks[i].storage.get()), chunkSize);
            }
        }

        void Save(BinaryOutputArchive& bin) const
        {
            bin.SaveValue(version);
            bin.SaveValue(chunkSize);

            bin.SaveValue(static_cast<uint32_t>(cells.size()));
            bin.SaveValue(reinterpret_cast<const char *>(cells.data()), cells.size() * sizeof(PVSCell));

            bin.SaveValue(static_cast<uint32_t>(chunks.size()));
            for (const auto &chunk : chunks) {
                bin.SaveValue(reinterpret_cast<const char *>(chunk.storage.get()), chunkSize);
            }
        }
        uint32_t version = 0;
        uint32_t chunkSize = 0;

        // PVSSectorCoord coord;
        std::vector<PVSCell> cells;
        std::vector<PVSChunk> chunks;
    };

    class PVSSectorProvider {
    public:
        explicit PVSSectorProvider(const FilePath& inPath) : basePath(inPath) {}
        ~PVSSectorProvider() = default;

        static std::string HeaderFileName() noexcept;
        static std::string SectorFileName(const PVSSectorCoord &coord) noexcept;

        bool LoadHeader(PVSConfig &config);
        bool LoadSector(const PVSSectorCoord &coord, PVSSector &outSector);

        FilePath basePath;
    };

    /**
     * @brief Streaming configuration for the PVS loader.
     *
     * Uses separate load and unload radii to prevent oscillation (thrashing)
     * when the viewer is near a sector boundary.
     *
     * - loadRadius:   sectors within this Chebyshev distance are streamed in.
     * - unloadMargin: extra margin added on top of loadRadius for unloading.
     *                 A sector is only unloaded when its distance exceeds
     *                 (loadRadius + unloadMargin), creating a hysteresis band.
     */
    struct PVSStreamingConfig {
        int32_t loadRadius   = 1;
        int32_t unloadMargin = 1;
    };

    class PVSLoader {
    public:
        explicit PVSLoader(const PVSConfig& inCfg) : config(inCfg) {}
        ~PVSLoader() = default;

        const PVSConfig& GetConfig() const { return config; }

        /**
         * @brief Update streaming state based on the current viewer position.
         *
         * Calculates the current sector coordinate, streams in sectors within
         * the load radius, and unloads sectors beyond (loadRadius + unloadMargin).
         */
        void Update(const Vector3& pos);

        void SetProvider(PVSSectorProvider* provider)
        {
            selectorProvider.reset(provider);
        }

        /**
         * @brief Set streaming configuration.
         *
         * @param cfg Streaming config with load radius and unload margin.
         *            unloadMargin >= 0 provides hysteresis to prevent
         *            load/unload oscillation at sector boundaries.
         */
        void SetStreamingConfig(const PVSStreamingConfig &cfg) { streamingConfig = cfg; }

        /**
         * @brief Get the current streaming configuration.
         */
        const PVSStreamingConfig& GetStreamingConfig() const { return streamingConfig; }

        /**
         * @brief Find a loaded sector by its coordinate.
         * @return Pointer to the sector, or nullptr if not loaded.
         */
        const PVSSector* FindSector(const PVSSectorCoord &coord) const;

        /**
         * @brief Query the visibility data for a given cell within a sector.
         * @return Pointer to the raw visibility bitfield data, or nullptr if unavailable.
         */
        const uint8_t* QueryVisibility(const PVSCellCoord &cellCoord) const;

    private:
        /**
         * @brief Chebyshev distance between two 2D sector coordinates.
         */
        static int32_t SectorDistance(const PVSSectorCoord &a, const PVSSectorCoord &b)
        {
            int32_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
            int32_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
            return dx > dy ? dx : dy;
        }

        /**
         * @brief Check if a sector with the given coordinate is already loaded.
         */
        bool IsSectorLoaded(const PVSSectorCoord &coord) const;

        /**
         * @brief Load/stream-in a sector at the given coordinate.
         */
        void LoadSector(const PVSSectorCoord &coord);

        /**
         * @brief Unload/stream-out a sector at the given coordinate.
         */
        void UnloadSector(const PVSSectorCoord &coord);

        // config
        PVSConfig config;
        PVSStreamingConfig streamingConfig;

        // cache
        PVSSectorCoord currentSectorCoord = {};
        const PVSSector *currentSector = nullptr;

        // data
        std::unique_ptr<PVSSectorProvider> selectorProvider;
        std::unordered_map<PVSSectorCoord, PVSSector, PVSSectorCoordHash> loadedSectors;
    };

} // namespace sky