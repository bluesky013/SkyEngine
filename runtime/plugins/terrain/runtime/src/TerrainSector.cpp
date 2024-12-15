//
// Created by blues on 2024/11/26.
//

#include <terrain/TerrainSector.h>
#include <terrain/TerrainFeature.h>

namespace sky {

    TerrainSectorRender::TerrainSectorRender(const TerrainCoord &coord)
        : sectorCoord(coord)
        , primitive(std::make_unique<RenderPrimitive>())
    {
    }

    void TerrainSectorRender::SetMaterial(const RDMaterialInstancePtr &mat, const RDResourceGroupPtr &batch)
    {
        primitive->techniques.clear();
        const auto &techniques = mat->GetMaterial()->GetGfxTechniques();
        primitive->techniques.reserve(techniques.size());
        for (const auto &tech : techniques) {
            TechniqueInstance inst = {tech, mat};
            inst.shaderOption = new ShaderOption();
            tech->Process(primitive->vertexFlags, inst.shaderOption);
            primitive->techniques.emplace_back(inst);
        }
        primitive->batchSet = batch;
        primitive->instanceSet = TerrainFeature::Get()->RequestResourceGroup();
    }

    void TerrainSectorRender::SetHeightMap(const RDTexture2DPtr &heightMap)
    {
        primitive->instanceSet->BindTexture("HeightMap", heightMap->GetImageView(), 0);
        primitive->instanceSet->Update();
    }

    void TerrainSectorRender::SetGeometry(const RenderGeometryPtr &geom, uint32_t instanceId)
    {
        rhi::CmdDrawIndexed dc = {};
        uint32_t indexStride = geom->indexBuffer.indexType == rhi::IndexType::U16 ? 2 : 4;
        dc.indexCount = static_cast<uint32_t>(geom->indexBuffer.range / indexStride);
        dc.firstInstance = instanceId;

        primitive->geometry = geom;
        primitive->args.emplace_back(dc);
        primitive->isReady = true;
    }
} // namespace sky