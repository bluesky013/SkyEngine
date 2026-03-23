//
// Created on 2025/01/15.
//

#pragma once

#include <terrain/ITerrainRenderer.h>

namespace sky {

    // Placeholder for GPU-driven rendering (Phase 3).
    // All methods log a warning and do nothing.
    class TerrainRenderGPU : public ITerrainRenderer {
    public:
        TerrainRenderGPU() = default;
        ~TerrainRenderGPU() override = default;

        void Init(RenderScene *scene, TerrainClipmap *clipmap) override;
        void SetMaterial(const RDMaterialInstancePtr &material) override;
        void SetTileTextures(const RDTexture2DPtr &heightmap, const RDTexture2DPtr &splatmap) override;
        void UpdateClipmap(const Vector3 &cameraPos) override;
        void Tick(float time) override;
        void Render() override;
        void Shutdown() override;
    };

} // namespace sky
