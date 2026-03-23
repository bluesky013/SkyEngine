//
// Created on 2025/01/15.
//

#pragma once

#include <terrain/ITerrainRenderer.h>
#include <render/RenderPrimitive.h>
#include <render/mesh/MeshPrimitive.h>
#include <render/resource/ResourceGroup.h>
#include <render/resource/Buffer.h>
#include <render/SceneView.h>

namespace sky {

    // A render primitive for one clipmap level.
    // Uses instanced draw (one block mesh × N visible blocks).
    struct TerrainLevelPrimitive : RenderPrimitive {
        ~TerrainLevelPrimitive() override = default;

        void SetMaterial(const RDMaterialInstancePtr &mat);
        void SetInstanceCount(uint32_t count, uint32_t indexCount);

        bool PrepareBatch(const RenderBatchPrepareInfo &info) noexcept override;
        void GatherRenderItem(IRenderItemGatherContext *context) noexcept override;

        RDMaterialInstancePtr material;
        RDResourceGroupPtr    instanceSet;
        RDBufferPtr           instanceDataSSBO; // StructuredBuffer<ClipmapBlock> for this level

        std::vector<std::unique_ptr<RenderBatch>> batches;
        rhi::CmdDrawIndexed drawCmd = {};
    };

    class TerrainRendererCPU : public ITerrainRenderer {
    public:
        TerrainRendererCPU() = default;
        ~TerrainRendererCPU() override;

        void Init(RenderScene *scene, TerrainClipmap *clipmap) override;
        void SetMaterial(const RDMaterialInstancePtr &material) override;
        void SetTileTextures(const RDTexture2DPtr &heightmap, const RDTexture2DPtr &splatmap) override;
        void UpdateClipmap(const Vector3 &cameraPos) override;
        void Tick(float time) override;
        void Render() override;
        void Shutdown() override;

    private:
        void BuildInstanceBuffer(uint32_t level, const SceneView *view);
        void AttachToScene();
        void DetachFromScene();

        RenderScene   *scene   = nullptr;
        TerrainClipmap *clipmap = nullptr;

        RDMaterialInstancePtr material;

        // Tile textures for all levels (shared for now; per-tile atlas in Phase 2)
        RDTexture2DPtr heightmapTexture;
        RDTexture2DPtr splatmapTexture;
        rhi::SamplerPtr tileSampler;

        // Per-level instance data buffers
        std::vector<VertexBuffer> instanceBuffers;

        // Per-level instance data SSBOs
        std::vector<RDBufferPtr> instanceDataBuffers;

        // One primitive per clipmap level
        std::vector<std::unique_ptr<TerrainLevelPrimitive>> primitives;

        bool attached = false;
    };

} // namespace sky
