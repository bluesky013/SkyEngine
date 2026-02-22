//
// Created by blues on 2024/9/21.
//

#pragma once

#include <render/debug/DebugRenderer.h>
#include <render/RenderPrimitive.h>
#include <core/template/ReferenceObject.h>

namespace sky {

    class Grid : public RefObject {
    public:
        Grid();
        ~Grid() override = default;

        void Draw(float gridSize);
        void SetTechnique(const RDGfxTechPtr &tech);

        RenderPrimitive* GetPrimitive() const { return primitive.get(); }
    private:
        std::unique_ptr<DebugRenderer> renderer;
        std::unique_ptr<RenderPrimitive> primitive;
    };
    using GizmoPtr = CounterPtr<Grid>;
} // namespace sky
