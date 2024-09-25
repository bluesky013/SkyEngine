//
// Created by blues on 2024/9/21.
//

#include <render/debug/Gizmo.h>

namespace sky {

    Gizmo::Gizmo()
        : renderer(new DebugRenderer())
        , primitive(new RenderPrimitive())
    {
    }

    void Gizmo::DrawGrid()
    {

    }

    void Gizmo::SetTechnique(const RDGfxTechPtr &tech)
    {
        TechniqueInstance techInst = {tech};
        primitive->techniques.clear();
        primitive->techniques.emplace_back(techInst);
    }
} // namespace sky