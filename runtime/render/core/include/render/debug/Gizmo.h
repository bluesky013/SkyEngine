//
// Created by blues on 2024/9/21.
//

#pragma once

#include <render/debug/DebugRenderer.h>
#include <render/RenderPrimitive.h>
#include <core/template/ReferenceObject.h>

namespace sky {

    class Gizmo : public RefObject {
    public:
        Gizmo();
        ~Gizmo() override = default;

        void DrawGrid();
        void SetTechnique(const RDGfxTechPtr &tech);

    private:
        std::unique_ptr<DebugRenderer> renderer;
        std::unique_ptr<RenderPrimitive> primitive;
    };
    using GizmoPtr = CounterPtr<Gizmo>;
} // namespace sky
