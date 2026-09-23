//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainAsset.h>
#include <terrain/TerrainSource.h>

namespace sky::terrain {

    // Generates a full terrain asset (all tiles + manifest) from an authored source.
    // Returns false when the source has an invalid layout. Render/physics-free.
    bool BuildTerrainAsset(const TerrainSourceData &source, TerrainAssetData &out);

} // namespace sky::terrain
