//
// Created on 2025/01/15.
//

#include <terrain/TerrainRenderGPU.h>
#include <core/logger/Logger.h>

static auto TAG = sky::Logger::Tag("TerrainRenderGPU");

namespace sky {

    void TerrainRenderGPU::Init(RenderScene * /*scene*/, TerrainClipmap * /*clipmap*/)
    {
        LOG_W(TAG, "TerrainRenderGPU is a placeholder — GPU-driven terrain not yet implemented.");
    }

    void TerrainRenderGPU::SetMaterial(const RDMaterialInstancePtr & /*material*/)
    {
    }

    void TerrainRenderGPU::SetTileTextures(const RDTexture2DPtr & /*heightmap*/, const RDTexture2DPtr & /*splatmap*/)
    {
    }

    void TerrainRenderGPU::UpdateClipmap(const Vector3 & /*cameraPos*/)
    {
    }

    void TerrainRenderGPU::Tick(float /*time*/)
    {
    }

    void TerrainRenderGPU::Render()
    {
    }

    void TerrainRenderGPU::Shutdown()
    {
    }

} // namespace sky
