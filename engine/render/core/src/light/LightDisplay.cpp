//
// Created by Zach Lee on 2026/3/8.
//

#include <render/light/LightDisplay.h>
#include <cmath>

namespace sky {

    static constexpr uint32_t CIRCLE_SEGMENTS  = 32;
    static constexpr uint32_t ARROW_RAY_COUNT  = 8;
    static constexpr float    ARROW_LENGTH     = 3.0f;
    static constexpr float    SUN_DISK_RADIUS  = 1.5f;
    static constexpr float    ARROW_RAY_OFFSET = 2.0f;

    struct LightGizmoPrimitive : RenderPrimitive {};

    LightGizmoDisplay::LightGizmoDisplay()
        : renderer(std::make_unique<DebugRenderer>())
        , primitive(std::make_unique<LightGizmoPrimitive>())
    {
    }

    void LightGizmoDisplay::SetTechnique(const RDGfxTechPtr &tech)
    {
        // RenderBatch batch = {tech};
        // batch.topo = rhi::PrimitiveTopology::LINE_LIST;
        // primitive->sections.clear();
        // primitive->sections.emplace_back();
        // primitive->sections.back().batches.clear();
        // primitive->sections.back().batches.emplace_back(batch);
    }

    void LightGizmoDisplay::BuildBasis(const Vector3 &dir, Vector3 &outRight, Vector3 &outUp)
    {
        Vector3 d = dir;
        d.Normalize();

        // Pick a helper vector that is not parallel to dir
        Vector3 helper = (std::fabs(d.y) < 0.99f) ? VEC3_Y : VEC3_X;
        outRight = d.Cross(helper);
        outRight.Normalize();
        outUp = outRight.Cross(d);
        outUp.Normalize();
    }

    void LightGizmoDisplay::DrawCircle(const Vector3 &center, const Vector3 &normal, float radius, uint32_t segments, const Color32 &color)
    {
        Vector3 right, up;
        BuildBasis(normal, right, up);

        float step = 2.0f * PI / static_cast<float>(segments);
        renderer->SetColor(color);

        Vector3 prev = center + right * radius;
        for (uint32_t i = 1; i <= segments; ++i) {
            float angle = step * static_cast<float>(i);
            Vector3 curr = center + right * (radius * std::cos(angle)) + up * (radius * std::sin(angle));
            renderer->DrawLine(prev, curr);
            prev = curr;
        }
    }

    void LightGizmoDisplay::DrawDirectionArrows(const Vector3 &center, const Vector3 &direction, float length, float radius, uint32_t rayCount, const Color32 &color)
    {
        Vector3 dir = direction;
        dir.Normalize();

        Vector3 right, up;
        BuildBasis(dir, right, up);

        renderer->SetColor(color);

        float step = 2.0f * PI / static_cast<float>(rayCount);
        for (uint32_t i = 0; i < rayCount; ++i) {
            float angle = step * static_cast<float>(i);
            Vector3 offset = right * (radius * std::cos(angle)) + up * (radius * std::sin(angle));
            Vector3 start = center + offset;
            Vector3 end   = start + dir * length;
            renderer->DrawLine(start, end);
        }
    }

    void LightGizmoDisplay::DrawCone(const Vector3 &apex, const Vector3 &direction, float angle, float height, uint32_t segments, const Color32 &color)
    {
        Vector3 dir = direction;
        dir.Normalize();

        Vector3 right, up;
        BuildBasis(dir, right, up);

        float baseRadius = height * std::tan(angle);
        Vector3 baseCenter = apex + dir * height;

        // Draw the base circle
        DrawCircle(baseCenter, dir, baseRadius, segments, color);

        // Draw lines from apex to base circle
        renderer->SetColor(color);
        float step = 2.0f * PI / static_cast<float>(segments);
        for (uint32_t i = 0; i < segments; i += (segments / 4)) {
            float a = step * static_cast<float>(i);
            Vector3 basePoint = baseCenter + right * (baseRadius * std::cos(a)) + up * (baseRadius * std::sin(a));
            renderer->DrawLine(apex, basePoint);
        }
    }

    // -----------------------------------------------------------------------
    // DirectLight: sun disk + parallel arrow rays along direction
    // -----------------------------------------------------------------------
    void LightGizmoDisplay::DrawDirectLight(const Vector3 &direction, const Vector3 &position, const Color32 &color)
    {
        renderer->Reset();

        Vector3 dir = direction;
        dir.Normalize();

        // Sun disk circle perpendicular to direction
        DrawCircle(position, dir, SUN_DISK_RADIUS, CIRCLE_SEGMENTS, color);

        // Cross lines through the disk center
        {
            Vector3 right, up;
            BuildBasis(dir, right, up);
            renderer->SetColor(color);
            renderer->DrawLine(position + right * SUN_DISK_RADIUS, position - right * SUN_DISK_RADIUS);
            renderer->DrawLine(position + up * SUN_DISK_RADIUS, position - up * SUN_DISK_RADIUS);
        }

        // Arrow rays starting from the edge of the disk going along direction
        DrawDirectionArrows(position + dir * ARROW_RAY_OFFSET, dir, ARROW_LENGTH, SUN_DISK_RADIUS, ARROW_RAY_COUNT, color);

        renderer->Render(primitive.get());
    }

    // -----------------------------------------------------------------------
    // PointLight: three mutually-perpendicular wireframe circles
    // -----------------------------------------------------------------------
    void LightGizmoDisplay::DrawPointLight(const Vector3 &position, float range, const Color32 &color)
    {
        renderer->Reset();

        // XY plane circle
        DrawCircle(position, VEC3_Z, range, CIRCLE_SEGMENTS, color);
        // XZ plane circle
        DrawCircle(position, VEC3_Y, range, CIRCLE_SEGMENTS, color);
        // YZ plane circle
        DrawCircle(position, VEC3_X, range, CIRCLE_SEGMENTS, color);

        // Small cross at center
        float s = range * 0.1f;
        renderer->SetColor(color);
        renderer->DrawLine(position - VEC3_X * s, position + VEC3_X * s);
        renderer->DrawLine(position - VEC3_Y * s, position + VEC3_Y * s);
        renderer->DrawLine(position - VEC3_Z * s, position + VEC3_Z * s);

        renderer->Render(primitive.get());
    }

    // -----------------------------------------------------------------------
    // SpotLight: cone showing angle + range, plus a small sphere at apex
    // -----------------------------------------------------------------------
    void LightGizmoDisplay::DrawSpotLight(const Vector3 &position, const Vector3 &direction, float angle, float range, const Color32 &color)
    {
        renderer->Reset();

        Vector3 dir = direction;
        dir.Normalize();

        // Main cone
        DrawCone(position, dir, angle, range, CIRCLE_SEGMENTS, color);

        // Half-range inner circle to show the cone body
        float halfRange = range * 0.5f;
        float halfRadius = halfRange * std::tan(angle);
        Vector3 halfCenter = position + dir * halfRange;
        DrawCircle(halfCenter, dir, halfRadius, CIRCLE_SEGMENTS, color);

        // Small source indicator at apex
        float s = range * 0.05f;
        renderer->SetColor(color);
        renderer->DrawLine(position - VEC3_X * s, position + VEC3_X * s);
        renderer->DrawLine(position - VEC3_Y * s, position + VEC3_Y * s);
        renderer->DrawLine(position - VEC3_Z * s, position + VEC3_Z * s);

        renderer->Render(primitive.get());
    }

} // namespace sky
