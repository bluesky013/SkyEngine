//
// Created on 2026/10/22.
//

#pragma once

#include <array>
#include <core/math/Transform.h>
#include <editor/core/viewport/EditorCamera.h>
#include <string>
#include <vector>

namespace sky::editor {
    enum class GizmoTool { SELECT, TRANSLATE, ROTATE, SCALE };
    enum class GizmoHandle { NONE, X, Y, Z, XY, XZ, YZ, UNIFORM };
    struct GizmoStyle {
        float                   length = 80, width = 2, hitRadius = 7;
        std::array<uint32_t, 3> axes{};
        uint32_t                center = 0, highlight = 0;
    };
    struct GizmoDrawContext {
        EditorCamera           camera;
        Vector2                extent;
        Transform              world;
        GizmoTool              tool  = GizmoTool::TRANSLATE;
        bool                   local = false;
        GizmoStyle             style;
        std::array<Vector3, 3> Axes() const;
    };
    struct GizmoPrimitive {
        GizmoHandle          handle = GizmoHandle::NONE;
        std::vector<Vector2> points;
        uint32_t             color = 0;
        float                width = 1;
        // Fills always close the contour; closed only controls polyline strokes.
        bool filled = false, closed = false;
    };
    using GizmoGeometry = std::vector<GizmoPrimitive>;

    // Providers return owned viewport-local layout geometry; fills must be convex.
    // Split concave shapes into convex primitives with the same handle for matching painting/picking.
    class IGizmoVisualProvider {
    public:
        virtual ~IGizmoVisualProvider()                                                                      = default;
        virtual bool Build(const GizmoDrawContext &context, GizmoGeometry &output, std::string &error) const = 0;
    };
    class DefaultGizmoVisualProvider final : public IGizmoVisualProvider {
    public:
        bool Build(const GizmoDrawContext &context, GizmoGeometry &output, std::string &error) const override;
    };
    bool        ValidateGizmoGeometry(const GizmoGeometry &geometry, std::string &error);
    GizmoHandle HitTestGizmo(const GizmoGeometry &geometry, Vector2 point, float tolerance);
    bool        ValidGizmoTransform(const Transform &transform);

    class GizmoDrag {
    public:
        bool Begin(const GizmoDrawContext &context,
                   GizmoHandle             handle,
                   Vector2                 pointer,
                   const Transform        &local,
                   const Transform        &parent,
                   std::string            &error);
        bool Update(Vector2 pointer, bool snap, const ViewportControlSettings &settings, Transform &result) const;

    private:
        bool             Intersect(Vector2 pointer, Vector3 &point) const;
        GizmoDrawContext context;
        GizmoHandle      handle = GizmoHandle::NONE;
        Transform        before, parent;
        Vector2          startPointer;
        Vector3          normal, startPoint;
        float            worldLength = 1;
    };
} // namespace sky::editor
