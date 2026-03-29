//
// Created on 2025/01/15.
//

#include <terrain/TerrainRendererCPU.h>
#include <terrain/TerrainFeature.h>
#include <render/RenderScene.h>
#include <render/RHI.h>
#include <core/name/Name.h>
#include <core/shapes/AABB.h>

namespace sky {

    // ==================== TerrainLevelPrimitive ====================

    void TerrainLevelPrimitive::SetMaterial(const RDMaterialInstancePtr &mat)
    {
        material = mat;
        batches.clear();

        const auto &techniques = mat->GetMaterial()->GetGfxTechniques();
        batches.reserve(techniques.size());
        for (const auto &tech : techniques) {
            auto batch = std::make_unique<RenderBatch>(tech);
            batch->geometry = geometry.Get();
            batch->material = mat.Get();
            batches.emplace_back(std::move(batch));
        }
    }

    void TerrainLevelPrimitive::SetInstanceCount(uint32_t count, uint32_t indexCount)
    {
        drawCmd.indexCount    = indexCount;
        drawCmd.instanceCount = count;
        drawCmd.firstIndex    = 0;
        drawCmd.vertexOffset  = 0;
        drawCmd.firstInstance = 0;
    }

    bool TerrainLevelPrimitive::PrepareBatch(const RenderBatchPrepareInfo &info) noexcept
    {
        bool result = false;
        for (auto &batch : batches) {
            if (batch->GetRasterID() == info.techId) {
                batch->Prepare(info);

                // Bind instance data SSBO to batch group after Prepare creates it
                if (batch->batchGroup && instanceDataSSBO) {
                    batch->batchGroup->BindBuffer(Name("InstancedData"),
                        instanceDataSSBO->GetRHIBuffer(), 0);
                    batch->batchGroup->Update();
                }

                result = true;
            }
        }

        if (result && geometry) {
            geometry->Upload();
        }
        return result;
    }

    void TerrainLevelPrimitive::GatherRenderItem(IRenderItemGatherContext *context) noexcept
    {
        if (drawCmd.instanceCount == 0) {
            return;
        }

        for (auto &batch : batches) {
            if (batch->GetRasterID() == context->rasterID) {
                context->Append(RenderItem{
                    .geometry   = geometry,
                    .batchGroup = batch->batchGroup,
                    .instanceSet = instanceSet,
                    .pso        = batch->pso,
                    .args       = {drawCmd}
                });
            }
        }
    }

    // ==================== TerrainRendererCPU ====================

    TerrainRendererCPU::~TerrainRendererCPU()
    {
        Shutdown();
    }

    void TerrainRendererCPU::Init(RenderScene *scn, TerrainClipmap *cm)
    {
        scene   = scn;
        clipmap = cm;

        uint32_t numLevels = clipmap->GetNumLevels();
        primitives.resize(numLevels);
        instanceBuffers.resize(numLevels);
        instanceDataBuffers.resize(numLevels);

        // Max blocks per level: BLOCKS_PER_SIDE^2 = 16
        static constexpr uint32_t MAX_BLOCKS = 16;

        auto *device = RHI::Get()->GetDevice();
        auto *queue  = device->GetQueue(rhi::QueueType::TRANSFER);

        for (uint32_t L = 0; L < numLevels; ++L) {
            // Create instance vertex buffer (per-instance uint32 IDs)
            uint32_t ibSize = MAX_BLOCKS * sizeof(uint32_t);
            instanceBuffers[L].buffer = new Buffer();
            instanceBuffers[L].buffer->Init(ibSize,
                rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                rhi::MemoryType::CPU_TO_GPU);
            instanceBuffers[L].offset = 0;
            instanceBuffers[L].range  = ibSize;
            instanceBuffers[L].stride = sizeof(uint32_t);

            // Create instance data SSBO
            uint32_t ssboSize = MAX_BLOCKS * sizeof(ClipmapBlockGPU);
            instanceDataBuffers[L] = new Buffer();
            instanceDataBuffers[L]->Init(ssboSize,
                rhi::BufferUsageFlagBit::STORAGE | rhi::BufferUsageFlagBit::TRANSFER_DST,
                rhi::MemoryType::CPU_TO_GPU);

            // Create geometry for this level (shared VB + per-level instance VB)
            CounterPtr<RenderGeometry> geom = new RenderGeometry();
            geom->vertexBuffers.emplace_back(clipmap->GetVertexBuffer());
            geom->vertexBuffers.emplace_back(instanceBuffers[L]);
            geom->indexBuffer = clipmap->GetIndexBuffer();

            geom->vertexAttributes.emplace_back(VertexAttribute{
                VertexSemanticFlagBit::POSITION, 0, 0, rhi::Format::U_RG8});
            geom->vertexAttributes.emplace_back(VertexAttribute{
                VertexSemanticFlagBit::INST0, 1, 0, rhi::Format::U_R32});
            geom->attributeSemantics = VertexSemanticFlagBit::POSITION | VertexSemanticFlagBit::INST0;

            // Create primitive
            primitives[L] = std::make_unique<TerrainLevelPrimitive>();
            primitives[L]->geometry = geom;
            primitives[L]->shouldUseFrustumCulling = false; // We do our own culling
            primitives[L]->instanceSet = TerrainFeature::Get()->RequestResourceGroup();
            primitives[L]->instanceDataSSBO = instanceDataBuffers[L];
        }

        // Create shared sampler for heightmap/splatmap
        rhi::Sampler::Descriptor samplerDesc = {};
        samplerDesc.minFilter = rhi::Filter::LINEAR;
        samplerDesc.magFilter = rhi::Filter::LINEAR;
        samplerDesc.addressModeU = rhi::WrapMode::CLAMP_TO_EDGE;
        samplerDesc.addressModeV = rhi::WrapMode::CLAMP_TO_EDGE;
        samplerDesc.addressModeW = rhi::WrapMode::CLAMP_TO_EDGE;
        samplerDesc.maxLod = 13.f;
        tileSampler = device->CreateSampler(samplerDesc);
    }

    void TerrainRendererCPU::SetMaterial(const RDMaterialInstancePtr &mat)
    {
        material = mat;
        for (auto &prim : primitives) {
            prim->SetMaterial(mat);
        }
    }

    void TerrainRendererCPU::SetTileTextures(const RDTexture2DPtr &heightmap, const RDTexture2DPtr &splatmap)
    {
        heightmapTexture = heightmap;
        splatmapTexture  = splatmap;

        for (auto &prim : primitives) {
            if (!prim->instanceSet) continue;

            if (heightmapTexture) {
                prim->instanceSet->BindTexture(Name("HeightMap"),
                    heightmapTexture->GetImageView(), tileSampler, 0);
            }
            if (splatmapTexture) {
                prim->instanceSet->BindTexture(Name("SplatMap"),
                    splatmapTexture->GetImageView(), tileSampler, 0);
            }
            prim->instanceSet->Update();
        }
    }

    void TerrainRendererCPU::UpdateClipmap(const Vector3 &cameraPos)
    {
        clipmap->UpdateSnapPositions(cameraPos);

        // Get main scene view for frustum culling
        const SceneView *mainView = nullptr;
        if (scene) {
            mainView = scene->GetSceneView(Name("MainCamera"));
        }

        for (uint32_t L = 0; L < clipmap->GetNumLevels(); ++L) {
            BuildInstanceBuffer(L, mainView);
        }
    }

    void TerrainRendererCPU::BuildInstanceBuffer(uint32_t level, const SceneView *view)
    {
        const auto &blocks = clipmap->GetBlocksForLevel(level);

        // Filter blocks by frustum culling
        std::vector<ClipmapBlockGPU> visibleBlocks;
        visibleBlocks.reserve(blocks.size());

        float blockWorldExtent = static_cast<float>(clipmap->GetBlockVertexCount() - 1);
        float heightMin = clipmap->GetLevels()[level].scale * -1.0f; // conservative lower bound
        float heightMax = 512.0f; // conservative upper bound (heightScale * max texel value)

        for (const auto &block : blocks) {
            if (view) {
                float extent = blockWorldExtent * block.scale;
                AABB aabb(
                    Vector3(block.offsetX, heightMin, block.offsetZ),
                    Vector3(block.offsetX + extent, heightMax, block.offsetZ + extent)
                );
                if (!view->FrustumCulling(aabb)) {
                    continue;
                }
            }
            visibleBlocks.emplace_back(block);
        }

        uint32_t blockCount = static_cast<uint32_t>(visibleBlocks.size());

        // Update instance data SSBO
        if (blockCount > 0) {
            auto *ptr = instanceDataBuffers[level]->GetRHIBuffer()->Map();
            memcpy(ptr, visibleBlocks.data(), blockCount * sizeof(ClipmapBlockGPU));
            instanceDataBuffers[level]->GetRHIBuffer()->UnMap();
        }

        // Update instance vertex buffer (just instance IDs 0..N-1)
        if (blockCount > 0) {
            auto *idPtr = reinterpret_cast<uint32_t*>(instanceBuffers[level].buffer->GetRHIBuffer()->Map());
            for (uint32_t i = 0; i < blockCount; ++i) {
                idPtr[i] = i;
            }
            instanceBuffers[level].buffer->GetRHIBuffer()->UnMap();
        }

        // Update draw command instance count
        primitives[level]->SetInstanceCount(blockCount, clipmap->GetBlockIndexCount());
    }

    void TerrainRendererCPU::Tick(float /*time*/)
    {
    }

    void TerrainRendererCPU::Render()
    {
        if (!attached) {
            AttachToScene();
        }
    }

    void TerrainRendererCPU::AttachToScene()
    {
        if (scene == nullptr || attached) { return; }
        for (auto &prim : primitives) {
            scene->AddPrimitive(prim.get());
        }
        attached = true;
    }

    void TerrainRendererCPU::DetachFromScene()
    {
        if (scene == nullptr || !attached) { return; }
        for (auto &prim : primitives) {
            scene->RemovePrimitive(prim.get());
        }
        attached = false;
    }

    void TerrainRendererCPU::Shutdown()
    {
        DetachFromScene();
        primitives.clear();
        instanceBuffers.clear();
        instanceDataBuffers.clear();
        material = nullptr;
    }

} // namespace sky
