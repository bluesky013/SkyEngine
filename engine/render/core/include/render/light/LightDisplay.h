//
// Created by Zach Lee on 2026/3/8.
//

#pragma once

#include <render/debug/DebugRenderer.h>
#include <render/RenderPrimitive.h>
#include <render/light/LightBase.h>
#include <core/math/MathUtil.h>
#include <memory>

namespace sky {

    class LightGizmoDisplay {
    public:
        LightGizmoDisplay();
        ~LightGizmoDisplay() = default;

        void SetTechnique(const RDGfxTechPtr &tech);

        // Draw gizmo for directional light
        void DrawDirectLight(const Vector3 &direction, const Vector3 &position, const Color32 &color = Color32(0xFF, 0xFF, 0x00, 0xFF));

        // Draw gizmo for point light
        void DrawPointLight(const Vector3 &position, float range, const Color32 &color = Color32(0xFF, 0xFF, 0x00, 0xFF));

        // Draw gizmo for spot light
        void DrawSpotLight(const Vector3 &position, const Vector3 &direction, float angle, float range, const Color32 &color = Color32(0xFF, 0xFF, 0x00, 0xFF));

        RenderPrimitive *GetPrimitive() const { return primitive.get(); }

    private:
        // Helper: draw a circle in a given plane at center, with given radius, normal, and segment count
        void DrawCircle(const Vector3 &center, const Vector3 &normal, float radius, uint32_t segments, const Color32 &color);

        // Helper: draw arrow rays from center along direction
        void DrawDirectionArrows(const Vector3 &center, const Vector3 &direction, float length, float radius, uint32_t rayCount, const Color32 &color);

        // Helper: draw a cone wireframe (for spot light)
        void DrawCone(const Vector3 &apex, const Vector3 &direction, float angle, float height, uint32_t segments, const Color32 &color);

        // Helper: build an orthonormal basis from a direction vector
        static void BuildBasis(const Vector3 &dir, Vector3 &outRight, Vector3 &outUp);

        std::unique_ptr<DebugRenderer> renderer;
        std::unique_ptr<RenderPrimitive> primitive;
    };

} // namespace sky