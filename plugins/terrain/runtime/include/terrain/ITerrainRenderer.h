//
// Created on 2025/01/15.
//

#pragma once

#include <render/resource/Material.h>
#include <render/resource/Texture.h>
#include <terrain/TerrainClipmap.h>

namespace sky {
    class RenderScene;

    class ITerrainRenderer {
    public:
        ITerrainRenderer() = default;
        virtual ~ITerrainRenderer() = default;

        virtual void Init(RenderScene *scene, TerrainClipmap *clipmap) = 0;
        virtual void SetMaterial(const RDMaterialInstancePtr &material) = 0;
        virtual void SetTileTextures(const RDTexture2DPtr &heightmap, const RDTexture2DPtr &splatmap) = 0;
        virtual void UpdateClipmap(const Vector3 &cameraPos) = 0;
        virtual void Tick(float time) = 0;
        virtual void Render() = 0;
        virtual void Shutdown() = 0;
    };

} // namespace sky
