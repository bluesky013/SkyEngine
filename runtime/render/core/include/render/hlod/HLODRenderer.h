//
// Created by Copilot on 2026/2/16.
//

#pragma once

#include <render/hlod/HLODTree.h>
#include <render/RenderPrimitive.h>
#include <core/math/Matrix4.h>
#include <memory>

namespace sky {
    class RenderScene;

    class HLODRenderer {
    public:
        HLODRenderer() = default;
        ~HLODRenderer();

        void AttachScene(RenderScene *scn);
        void SetHLODTree(const HLODTreePtr &tree);

        void UpdateTransform(const Matrix4 &matrix);
        void UpdateLODSelection(const Vector3 &cameraPos);

        void Tick();

    private:
        void RebuildPrimitives(const std::vector<uint32_t> &visibleNodes);
        void ClearPrimitives();

        RenderScene *scene = nullptr;

        HLODTreePtr hlodTree;
        Matrix4 worldMatrix;

        std::vector<uint32_t> activeNodes;
        std::vector<std::unique_ptr<RenderMaterialPrimitive>> primitives;
    };

} // namespace sky
