//
// Created by blues on 2024/11/26.
//

#include <terrain/TerrainSector.h>
#include <terrain/TerrainFeature.h>

namespace sky {

    TerrainSectorRender::TerrainSectorRender(const TerrainCoord &coord)
        : sectorCoord(coord)
        // , primitive(std::make_unique<RenderMaterialPrimitive>())
    {
    }

    void TerrainSectorRender::SetMaterial(const RDMaterialInstancePtr &mat, const RDResourceGroupPtr &batch)
    {
        primitive->sections.clear();
        const auto &techniques = mat->GetMaterial()->GetGfxTechniques();
        primitive->sections[0].batches.reserve(techniques.size());
        // primitive->1 = mat;
        for (const auto &tech : techniques) {
            // RenderBatch batch = {tech};
//            inst.shaderOption = new ShaderOption();
//            tech->Process(primitive->vertexFlags, inst.shaderOption);
            // primitive->batches.emplace_back(batch);
        }
//        primitive->batchSet = batch;
        primitive->instanceSet = TerrainFeature::Get()->RequestResourceGroup();
    }

    void TerrainSectorRender::SetHeightMap(const RDTexture2DPtr &heightMap)
    {
        primitive->instanceSet->BindTexture(Name("HeightMap"), heightMap->GetImageView(), 0);
        primitive->instanceSet->Update();
    }

    void TerrainSectorRender::SetGeometry(const RenderGeometryPtr &geom, uint32_t instanceId)
    {
        rhi::CmdDrawIndexed dc = {};
        uint32_t indexStride = geom->indexBuffer.indexType == rhi::IndexType::U16 ? 2 : 4;
        dc.indexCount = static_cast<uint32_t>(geom->indexBuffer.range / indexStride);
        dc.firstInstance = instanceId;
    }
} // namespace sky