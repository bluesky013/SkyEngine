//
// Created by Copilot on 2026/2/16.
//

#include <render/hlod/HLODRenderer.h>
#include <render/RenderScene.h>
#include <algorithm>

namespace sky {

    HLODRenderer::~HLODRenderer()
    {
        ClearPrimitives();
    }

    void HLODRenderer::AttachScene(RenderScene *scn)
    {
        scene = scn;
    }

    void HLODRenderer::SetHLODTree(const HLODTreePtr &tree)
    {
        ClearPrimitives();
        hlodTree = tree;
        activeNodes.clear();
    }

    void HLODRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        worldMatrix = matrix;
    }

    void HLODRenderer::UpdateLODSelection(const Vector3 &cameraPos)
    {
        if (!hlodTree) {
            return;
        }

        std::vector<uint32_t> visibleNodes;
        hlodTree->SelectLODs(cameraPos, visibleNodes);

        std::sort(visibleNodes.begin(), visibleNodes.end());

        if (visibleNodes != activeNodes) {
            RebuildPrimitives(visibleNodes);
            activeNodes = std::move(visibleNodes);
        }
    }

    void HLODRenderer::Tick()
    {
        for (auto &prim : primitives) {
            prim->worldBound = AABB::Transform(prim->localBound, worldMatrix);
        }
    }

    void HLODRenderer::ClearPrimitives()
    {
        if (scene != nullptr) {
            for (auto &prim : primitives) {
                scene->RemovePrimitive(prim.get());
            }
        }
        primitives.clear();
    }

    void HLODRenderer::RebuildPrimitives(const std::vector<uint32_t> &visibleNodes)
    {
        ClearPrimitives();

        if (!hlodTree || !scene) {
            return;
        }

        for (uint32_t nodeIdx : visibleNodes) {
            const auto &node = hlodTree->GetNode(nodeIdx);
            if (!node.mesh) {
                continue;
            }

            const auto &subMeshes = node.mesh->GetSubMeshes();
            const auto &geometry  = node.mesh->GetGeometry();

            for (const auto &subMesh : subMeshes) {
                auto prim = std::make_unique<RenderMaterialPrimitive>();
                prim->geometry = geometry;
                prim->localBound = subMesh.aabb;
                prim->worldBound = AABB::Transform(subMesh.aabb, worldMatrix);
                prim->material = subMesh.material;

                prim->args.emplace_back(rhi::CmdDrawIndexed{subMesh.indexCount, 1, subMesh.firstIndex, static_cast<int32_t>(subMesh.firstVertex), 0});

                scene->AddPrimitive(prim.get());
                primitives.emplace_back(std::move(prim));
            }
        }
    }

} // namespace sky
