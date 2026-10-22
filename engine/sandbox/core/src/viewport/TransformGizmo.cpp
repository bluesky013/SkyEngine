//
// Created on 2026/10/22.
//

#include <editor/core/viewport/TransformGizmo.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::editor {
    namespace {
        float Dot2(Vector2 a, Vector2 b)
        {
            return a.x * b.x + a.y * b.y;
        }
        float SegmentDistance(Vector2 p, Vector2 a, Vector2 b)
        {
            const auto  d       = b - a;
            const float length  = Dot2(d, d);
            const auto  nearest = a + d * (length > 1.e-8f ? std::clamp(Dot2(p - a, d) / length, 0.f, 1.f) : 0.f);
            return std::sqrt(Dot2(p - nearest, p - nearest));
        }
        bool Inside(Vector2 point, const std::vector<Vector2> &polygon)
        {
            bool inside = false;
            for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
                const auto a = polygon[i], b = polygon[j];
                if ((a.y > point.y) != (b.y > point.y) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
                    inside = !inside;
            }
            return inside;
        }
        bool Finite(Vector3 value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }
        bool ConvexPolygon(const std::vector<Vector2> &points)
        {
            if (points.size() < 3)
                return false;
            float winding = 0;
            for (size_t i = 0; i < points.size(); ++i) {
                const auto edge = points[(i + 1) % points.size()] - points[i];
                if (Dot2(edge, edge) < 1.e-8f)
                    return false;
                for (size_t j = 0; j < points.size(); ++j) {
                    const auto  offset = points[j] - points[i];
                    const float cross  = edge.x * offset.y - edge.y * offset.x;
                    if (std::abs(cross) < 1.e-4f)
                        continue;
                    if (winding * cross < 0)
                        return false;
                    winding = cross;
                }
            }
            return winding != 0;
        }
        std::array<int, 2> PlaneAxes(GizmoHandle handle)
        {
            if (handle == GizmoHandle::XY)
                return {0, 1};
            if (handle == GizmoHandle::XZ)
                return {0, 2};
            return {1, 2};
        }
        float Quantize(float value, float step, bool snap)
        {
            return snap && std::isfinite(step) && step > 0 ? std::round(value / step) * step : value;
        }
    } // namespace

    std::array<Vector3, 3> GizmoDrawContext::Axes() const
    {
        std::array<Vector3, 3> axes{Vector3{1, 0, 0}, Vector3{0, 1, 0}, Vector3{0, 0, 1}};
        if (local || tool == GizmoTool::SCALE) {
            for (auto &axis : axes)
                axis = world.rotation * axis;
        }
        return axes;
    }
    bool ValidGizmoTransform(const Transform &transform)
    {
        return Finite(transform.translation) && Finite(transform.scale) &&
               std::all_of(std::begin(transform.rotation.v), std::end(transform.rotation.v), [](float v) { return std::isfinite(v); }) &&
               std::abs(transform.rotation.Dot(transform.rotation) - 1.f) < .01f;
    }
    bool ValidateGizmoGeometry(const GizmoGeometry &geometry, std::string &error)
    {
        for (const auto &primitive : geometry) {
            if (primitive.points.size() < (primitive.filled ? 3u : 2u) || !std::isfinite(primitive.width) || primitive.width <= 0 ||
                primitive.handle < GizmoHandle::NONE || primitive.handle > GizmoHandle::UNIFORM ||
                std::any_of(primitive.points.begin(), primitive.points.end(), [](Vector2 p) { return !std::isfinite(p.x) || !std::isfinite(p.y); }) ||
                (primitive.filled && !ConvexPolygon(primitive.points))) {
                error = "Gizmo provider returned invalid geometry.";
                return false;
            }
        }
        error.clear();
        return true;
    }
    bool DefaultGizmoVisualProvider::Build(const GizmoDrawContext &context, GizmoGeometry &output, std::string &error) const
    {
        output.clear();
        error.clear();
        Vector2 center;
        if (context.tool == GizmoTool::SELECT || !context.camera.Project(context.world.translation, context.extent, center))
            return true;
        if (!ValidGizmoTransform(context.world)) {
            error = "Gizmo target transform is invalid.";
            return false;
        }
        const auto  axes    = context.Axes();
        const float length  = context.camera.WorldUnitsPerPixel(context.world.translation, context.extent.y) * context.style.length;
        const auto  project = [&](Vector3 offset, Vector2 &point) {
            return context.camera.Project(context.world.translation + offset * length, context.extent, point);
        };
        for (int i = 0; i < 3; ++i) {
            const auto     handle = static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::X) + i);
            GizmoPrimitive primitive{handle, {}, context.style.axes[i], context.style.width};
            if (context.tool == GizmoTool::ROTATE) {
                bool complete = true;
                for (int step = 0; step <= 64; ++step) {
                    const float angle = 6.283185307f * float(step) / 64.f;
                    Vector2     point;
                    if (!project(axes[(i + 1) % 3] * std::cos(angle) + axes[(i + 2) % 3] * std::sin(angle), point)) {
                        complete = false;
                        break;
                    }
                    primitive.points.push_back(point);
                }
                if (complete)
                    output.push_back(std::move(primitive));
                continue;
            }
            Vector2 end;
            if (!project(axes[i], end) || Dot2(end - center, end - center) < context.style.hitRadius * context.style.hitRadius)
                continue;
            primitive.points = {center, end};
            output.push_back(primitive);
            auto direction = end - center;
            direction      = direction * (1.f / std::sqrt(Dot2(direction, direction)));
            const Vector2 side{-direction.y, direction.x};
            const float   size = context.style.hitRadius;
            if (context.tool == GizmoTool::SCALE) {
                primitive.points = {end + Vector2{-size, -size}, end + Vector2{size, -size}, end + Vector2{size, size}, end + Vector2{-size, size}};
            } else {
                primitive.points = {end, end - direction * (size * 2.f) + side * size, end - direction * (size * 2.f) - side * size};
            }
            primitive.filled = primitive.closed = true;
            output.push_back(std::move(primitive));
        }
        if (context.tool == GizmoTool::TRANSLATE) {
            for (const auto handle : {GizmoHandle::XY, GizmoHandle::XZ, GizmoHandle::YZ}) {
                const auto     index = PlaneAxes(handle);
                GizmoPrimitive plane{handle, {}, context.style.axes[index[0]], context.style.width, true, true};
                bool           complete = true;
                for (const auto point : {Vector2{.2f, .2f}, Vector2{.4f, .2f}, Vector2{.4f, .4f}, Vector2{.2f, .4f}}) {
                    Vector2 screen;
                    if (!project(axes[index[0]] * point.x + axes[index[1]] * point.y, screen)) {
                        complete = false;
                        break;
                    }
                    plane.points.push_back(screen);
                }
                if (complete && ConvexPolygon(plane.points))
                    output.push_back(std::move(plane));
            }
        } else if (context.tool == GizmoTool::SCALE) {
            const float size = context.style.hitRadius;
            output.push_back({GizmoHandle::UNIFORM,
                              {center + Vector2{-size, 0}, center + Vector2{0, -size}, center + Vector2{size, 0}, center + Vector2{0, size}},
                              context.style.center,
                              context.style.width,
                              true,
                              true});
        }
        return ValidateGizmoGeometry(output, error);
    }
    GizmoHandle HitTestGizmo(const GizmoGeometry &geometry, Vector2 point, float tolerance)
    {
        GizmoHandle result  = GizmoHandle::NONE;
        float       nearest = std::numeric_limits<float>::max();
        for (auto it = geometry.rbegin(); it != geometry.rend(); ++it) {
            const auto &shape = *it;
            if (shape.handle == GizmoHandle::NONE || shape.points.size() < 2)
                continue;
            if (shape.filled && Inside(point, shape.points))
                return shape.handle;
            const size_t count = shape.points.size();
            for (size_t i = 0; i < (shape.filled || shape.closed ? count : count - 1); ++i) {
                const float distance = SegmentDistance(point, shape.points[i], shape.points[(i + 1) % count]);
                if (distance <= tolerance + shape.width * .5f && distance < nearest) {
                    nearest = distance;
                    result  = shape.handle;
                }
            }
        }
        return result;
    }
    bool GizmoDrag::Intersect(Vector2 pointer, Vector3 &point) const
    {
        const auto  ray         = context.camera.Ray(pointer, context.extent);
        const float denominator = ray.Dot(normal);
        if (std::abs(denominator) < 1.e-5f)
            return false;
        const float distance = (context.world.translation - context.camera.position).Dot(normal) / denominator;
        if (!std::isfinite(distance) || distance <= 0)
            return false;
        point = context.camera.position + ray * distance;
        return Finite(point);
    }
    bool GizmoDrag::Begin(const GizmoDrawContext &value,
                          GizmoHandle             selected,
                          Vector2                 pointer,
                          const Transform        &local,
                          const Transform        &parentTransform,
                          std::string            &error)
    {
        error.clear();
        if (value.tool == GizmoTool::SELECT || selected <= GizmoHandle::NONE || selected > GizmoHandle::UNIFORM ||
            !std::isfinite(value.style.length) || value.style.length <= 0 || value.extent.x <= 0 || value.extent.y <= 0 ||
            !ValidGizmoTransform(value.world) || !ValidGizmoTransform(local) || !ValidGizmoTransform(parentTransform) ||
            std::abs(parentTransform.scale.x) < 1.e-6f || std::abs(parentTransform.scale.y) < 1.e-6f || std::abs(parentTransform.scale.z) < 1.e-6f) {
            error = "Transform manipulation requires a valid target and an invertible parent.";
            return false;
        }
        if ((selected >= GizmoHandle::XY && selected <= GizmoHandle::YZ && value.tool != GizmoTool::TRANSLATE) ||
            (selected == GizmoHandle::UNIFORM && value.tool != GizmoTool::SCALE)) {
            error = "Handle is not supported by this tool.";
            return false;
        }
        context         = value;
        handle          = selected;
        before          = local;
        parent          = parentTransform;
        startPointer    = pointer;
        worldLength     = context.camera.WorldUnitsPerPixel(context.world.translation, context.extent.y) * context.style.length;
        const auto axes = context.Axes();
        if (handle == GizmoHandle::UNIFORM)
            return true;
        if (handle >= GizmoHandle::XY) {
            const auto index = PlaneAxes(handle);
            normal           = axes[index[0]].Cross(axes[index[1]]);
        } else {
            const auto axis = axes[static_cast<int>(handle) - static_cast<int>(GizmoHandle::X)];
            normal          = context.tool == GizmoTool::ROTATE ? axis : context.camera.Forward() - axis * context.camera.Forward().Dot(axis);
        }
        if (normal.Length() < 1.e-5f || !Intersect(pointer, startPoint)) {
            error = "Handle is aligned with the view; choose another axis or viewing angle.";
            return false;
        }
        return true;
    }
    bool GizmoDrag::Update(Vector2 pointer, bool snap, const ViewportControlSettings &settings, Transform &result) const
    {
        result          = before;
        const auto axes = context.Axes();
        Vector3    point;
        if (handle != GizmoHandle::UNIFORM && !Intersect(pointer, point))
            return false;
        const auto delta = point - startPoint;
        const int  index = handle >= GizmoHandle::X && handle <= GizmoHandle::Z ? static_cast<int>(handle) - static_cast<int>(GizmoHandle::X) : 0;
        if (context.tool == GizmoTool::TRANSLATE) {
            Vector3 movement;
            if (handle >= GizmoHandle::XY) {
                for (const auto axis : PlaneAxes(handle))
                    movement += axes[axis] * Quantize(delta.Dot(axes[axis]), settings.translationStep, snap);
            } else {
                movement = axes[index] * Quantize(delta.Dot(axes[index]), settings.translationStep, snap);
            }
            const auto position = context.world.translation + movement;
            result.translation  = (parent.rotation.Conjugate() * (position - parent.translation)) / parent.scale;
        } else if (context.tool == GizmoTool::ROTATE) {
            auto from = startPoint - context.world.translation, to = point - context.world.translation;
            if (from.Length() < 1.e-6f || to.Length() < 1.e-6f)
                return false;
            from.Normalize();
            to.Normalize();
            float angle     = std::atan2(axes[index].Dot(from.Cross(to)), from.Dot(to));
            angle           = Quantize(angle, settings.rotationStep * .01745329252f, snap);
            result.rotation = parent.rotation.Conjugate() * (Quaternion(angle, axes[index]) * context.world.rotation);
            result.rotation.Normalize();
        } else if (context.tool == GizmoTool::SCALE) {
            const float amount = handle == GizmoHandle::UNIFORM ? ((pointer.x - startPointer.x) - (pointer.y - startPointer.y)) / context.style.length
                                                                : delta.Dot(axes[index]) / std::max(worldLength, 1.e-6f);
            const float ratio  = std::max(.001f, 1.f + Quantize(amount, settings.scaleStep, snap));
            if (handle == GizmoHandle::UNIFORM)
                result.scale *= ratio;
            else
                result.scale[index] *= ratio;
        }
        return ValidGizmoTransform(result);
    }
} // namespace sky::editor
