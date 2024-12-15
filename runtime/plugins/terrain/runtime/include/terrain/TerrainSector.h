//
// Created by blues on 2024/11/26.
//

#pragma once

#include <terrain/TerrainBase.h>
#include <render/resource/Texture.h>
#include <render/RenderPrimitive.h>
#include <render/resource/ResourceGroup.h>
#include <core/math/Vector4.h>
#include <core/shapes/AABB.h>

namespace sky {

    struct TerrainSector {
        TerrainCoord   coord;
        RDTexture2DPtr heightMap;
    };

    struct TerrainInstanceData {
        float    offsetX;
        float    offsetY;
        uint32_t sectorWidth;
        uint32_t sectorHeight;
        float    minHeight;
        float    maxHeight;
        float    resolution;
        float    padding;
    };

    class TerrainSectorRender {
    public:
        explicit TerrainSectorRender(const TerrainCoord &coord);
        ~TerrainSectorRender() = default;

        void SetMaterial(const RDMaterialInstancePtr &mat, const RDResourceGroupPtr &batch);
        void SetGeometry(const RenderGeometryPtr &geom, uint32_t instanceId);
        void SetHeightMap(const RDTexture2DPtr &heightMap);

        RenderPrimitive* GetPrimitive() const { return primitive.get(); }
    private:
        TerrainCoord sectorCoord;
        std::unique_ptr<RenderPrimitive> primitive;
    };

} // namespace sky
