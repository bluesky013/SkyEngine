//
// Created by blues on 2024/11/26.
//

#pragma once

#include <terrain/TerrainBase.h>
#include <render/RenderPrimitive.h>
#include <render/RenderGeometry.h>
#include <render/resource/Texture.h>

namespace sky {
    class TerrainRender;

    class TerrainSector {
    public:
        explicit TerrainSector();
        ~TerrainSector();

    private:
        RenderGeometryPtr geometry;
        RDTexture2DPtr heightMap;
    };

} // namespace sky
