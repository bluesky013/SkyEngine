//
// Created by blues on 2024/11/26.
//

#pragma once

#include <terrain/TerrainBase.h>
#include <render/resource/Texture.h>
#include <render/RenderPrimitive.h>
#include <core/math/Vector4.h>
#include <core/shapes/AABB.h>

namespace sky {

    struct TerrainSector {
        TerrainCoord   coord;
        RDTexture2DPtr heightMap;
    };

    struct TerrainInstanceData {
        Vector4 worldOffset;
        Vector4 boxMin;
        Vector4 boxMax;
    };

    class TerrainSectorRender {
    public:
        explicit TerrainSectorRender(const TerrainCoord &coord);
        ~TerrainSectorRender() = default;

        void SetMaterial(const RDMaterialInstancePtr &mat);
        void SetGeometry(const RenderGeometryPtr &geom);

        RenderPrimitive* GetPrimitive() const { return primitive.get(); }
    private:
        TerrainCoord sectorCoord;
        std::unique_ptr<RenderPrimitive> primitive;
    };

} // namespace sky
