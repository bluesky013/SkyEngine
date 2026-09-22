//
// Created by Zach Lee on 2026/2/23.
//

#include "framework/asset/AssetDataBase.h"
#include "framework/asset/AssetManager.h"

#include <pvs/PVSLoader.h>

namespace sky {
    bool PVSSectorProvider::LoadSector(const PVSSectorCoord &coord, PVSSector &outSector)
    {
        FilePath sectorFile = basePath / FilePath(SectorFileName(coord));
        const auto &fs = AssetDataBase::Get()->GetWorkSpaceFs();
        auto file = fs->OpenFile(sectorFile);
        if (file) {
            auto archive = file->ReadAsArchive();
            BinaryInputArchive bin(*archive);
            outSector.Load(bin);
            return true;
        }
        return false;
    }

    std::string PVSSectorProvider::HeaderFileName() noexcept
    {
        return "resource.data";
    }

    std::string PVSSectorProvider::SectorFileName(const PVSSectorCoord &coord) noexcept
    {
        std::stringstream ss;
        ss << "PVS_Sector_" << coord.x << "_" << coord.y<< ".data";
        return ss.str();
    }

    bool PVSSectorProvider::LoadHeader(PVSConfig& config)
    {
        FilePath headerPath = basePath / FilePath(HeaderFileName());
        const auto &fs = AssetDataBase::Get()->GetWorkSpaceFs();
        auto headerFile = fs->OpenFile(headerPath);
        if (headerFile) {
            auto archive = headerFile->ReadAsArchive();
            BinaryInputArchive bin(*archive);
            config.Load(bin);
            return true;
        }
        return false;
    }

    bool PVSSectorWriter::WriteHeader(const PVSConfig &config)
    {
        const auto &fs = AssetDataBase::Get()->GetWorkSpaceFs();
        fs->MakeDir(basePath);

        FilePath headerPath = basePath / FilePath(PVSSectorProvider::HeaderFileName());
        auto headerFile = fs->CreateOrOpenFile(headerPath);
        if (headerFile) {
            auto archive = headerFile->WriteAsArchive();
            BinaryOutputArchive bin(*archive);
            config.Save(bin);
            return true;
        }
        return false;
    }

    bool PVSSectorWriter::WriteSector(const PVSSectorCoord &coord, const PVSSector &sector)
    {
        const auto &fs = AssetDataBase::Get()->GetWorkSpaceFs();
        fs->MakeDir(basePath);

        FilePath sectorPath = basePath / FilePath(PVSSectorProvider::SectorFileName(coord));
        auto sectorFile = fs->CreateOrOpenFile(sectorPath);
        if (sectorFile) {
            auto archive = sectorFile->WriteAsArchive();
            BinaryOutputArchive bin(*archive);
            sector.Save(bin);
            return true;
        }
        return false;
    }

    void PVSLoader::Update(const Vector3& pos)
    {
        auto newCoord = config.CalculateSectorCoordByWorldPosition(pos);
        if (newCoord == currentSectorCoord && !loadedSectors.empty()) {
            return;
        }
        currentSectorCoord = newCoord;

        const int32_t loadRadius   = streamingConfig.loadRadius;
        const int32_t unloadRadius = loadRadius + streamingConfig.unloadMargin;

        // Unload sectors beyond unload radius (hysteresis prevents thrashing)
        for (auto it = loadedSectors.begin(); it != loadedSectors.end(); ) {
            if (SectorDistance(it->first, currentSectorCoord) > unloadRadius) {
                it = loadedSectors.erase(it);
            } else {
                ++it;
            }
        }

        // Stream in sectors within load radius (2D)
        for (int32_t x = currentSectorCoord.x - loadRadius; x <= currentSectorCoord.x + loadRadius; ++x) {
            for (int32_t y = currentSectorCoord.y - loadRadius; y <= currentSectorCoord.y + loadRadius; ++y) {
                PVSSectorCoord coord{x, y};
                if (!IsSectorLoaded(coord)) {
                    LoadSector(coord);
                }
            }
        }

    }

    const PVSSector* PVSLoader::FindSector(const PVSSectorCoord &coord) const
    {
        auto it = loadedSectors.find(coord);
        return it != loadedSectors.end() ? &it->second : nullptr;
    }

    const uint8_t* PVSLoader::QueryVisibility(const PVSCellCoord &cellCoord) const
    {
        // Resolve the sector that actually contains the cell, so queries for cells
        // in streamed neighbor sectors are answered rather than silently dropped.
        const auto sectorCoord = config.CalculateSectorCoordByCellCoord(cellCoord);
        const PVSSector *sector = FindSector(sectorCoord);
        if (sector == nullptr) {
            return nullptr;
        }

        const uint32_t cellIndex = config.CalculateCellIndexInSector(cellCoord);
        if (cellIndex >= sector->cells.size()) {
            return nullptr;
        }

        const auto &cell = sector->cells[cellIndex];
        if (cell.chunkIndex >= sector->chunks.size() || sector->chunks[cell.chunkIndex].storage == nullptr) {
            return nullptr;
        }

        return sector->chunks[cell.chunkIndex].storage.get() + cell.dataOffset;
    }

    bool PVSLoader::IsSectorLoaded(const PVSSectorCoord &coord) const
    {
        return loadedSectors.count(coord) > 0;
    }

    void PVSLoader::LoadSector(const PVSSectorCoord &coord)
    {
        if (sectorProvider != nullptr) {
            PVSSector sector;

            if (sectorProvider->LoadSector(coord, sector)) {
                if (cellDataSize == 0 && config.cellsPerChunk > 0) {
                    cellDataSize = sector.chunkSize / config.cellsPerChunk;
                }
                loadedSectors.emplace(coord, std::move(sector));
            } else {
                // Record the miss but keep streaming; the update must not fail.
                ++missingSectorCount;
            }
        }
    }

    void PVSLoader::UnloadSector(const PVSSectorCoord &coord)
    {
        loadedSectors.erase(coord);
    }

} // namespace sky