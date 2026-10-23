//
// Created on 2026/10/23.
//

#include <algorithm>
#include <cmath>
#include <editor/core/viewport/IViewportPicker.h>

namespace sky::editor {
    namespace {
        float Depth(const EditorCamera &camera, Vector3 point)
        {
            return (point - camera.position).Dot(camera.Forward());
        }
        bool ClipSegment(const EditorCamera &camera, Vector3 &a, Vector3 &b)
        {
            float       low = 0, high = 1;
            const float da = Depth(camera, a), db = Depth(camera, b), delta = db - da;
            if (!std::isfinite(da) || !std::isfinite(db))
                return false;
            if (delta == 0)
                return da >= camera.nearPlane && da <= camera.farPlane;
            const float nearAt = (camera.nearPlane - da) / delta, farAt = (camera.farPlane - da) / delta;
            low  = std::max(low, std::min(nearAt, farAt));
            high = std::min(high, std::max(nearAt, farAt));
            if (low > high)
                return false;
            const auto start = a;
            a                = start + (b - start) * low;
            b                = start + (b - start) * high;
            return true;
        }
        bool Project(const EditorCamera &camera, Vector3 point, Vector2 extent, Vector2 &screen)
        {
            const float z = Depth(camera, point);
            if (!std::isfinite(z) || z <= 0 || extent.x <= 0 || extent.y <= 0)
                return false;
            const float scale    = extent.y / (2 * std::tan(camera.verticalFov * .5f) * z);
            const auto  relative = point - camera.position;
            screen               = {extent.x * .5f + relative.Dot(camera.Right()) * scale, extent.y * .5f - relative.Dot(camera.Up()) * scale};
            return std::isfinite(screen.x) && std::isfinite(screen.y);
        }
    } // namespace
    bool ProjectViewportPickProxy(const ViewportPickProxy &proxy, const EditorCamera &camera, Vector2 extent, ViewportProxyLayout &layout)
    {
        layout       = {};
        layout.color = proxy.color;
        layout.actor = proxy.target.actor;
        if (const auto *icon = std::get_if<ViewportBillboardProxy>(&proxy.shape)) {
            const float z = Depth(camera, icon->center);
            Vector2     center;
            if (z < camera.nearPlane || z > camera.farPlane || !Project(camera, icon->center, extent, center))
                return false;
            const auto half = icon->size * .5f;
            layout.points   = {center - half, Vector2{center.x + half.x, center.y - half.y}, center + half,
                               Vector2{center.x - half.x, center.y + half.y}};
            layout.count    = 4;
        } else if (const auto *line = std::get_if<ViewportSegmentProxy>(&proxy.shape)) {
            auto    a = line->a, b = line->b;
            Vector2 first, second;
            if (!ClipSegment(camera, a, b) || !Project(camera, a, extent, first) || !Project(camera, b, extent, second))
                return false;
            const auto  delta  = second - first;
            const float length = delta.Length();
            const auto  normal = length > 0 ? Vector2{-delta.y, delta.x} * (line->width * .5f / length) : Vector2{line->width * .5f, 0};
            layout.points      = {first + normal, second + normal, second - normal, first - normal};
            if (length == 0) {
                const Vector2 half{line->width * .5f, line->width * .5f};
                layout.points = {first - half, Vector2{first.x + half.x, first.y - half.y}, first + half,
                                 Vector2{first.x - half.x, first.y + half.y}};
            }
            layout.count = 4;
        } else {
            const auto            &triangle = std::get<ViewportTriangleProxy>(proxy.shape);
            std::array<Vector3, 6> points{triangle.a, triangle.b, triangle.c}, scratch{};
            uint32_t               count = 3;
            for (uint32_t plane = 0; plane < 2; ++plane) {
                uint32_t    written = 0;
                const float bound   = plane ? camera.farPlane : camera.nearPlane;
                for (uint32_t i = 0; i < count; ++i) {
                    const auto  a = points[i], b = points[(i + 1) % count];
                    const float da = Depth(camera, a), db = Depth(camera, b);
                    const bool  insideA = plane ? da <= bound : da >= bound, insideB = plane ? db <= bound : db >= bound;
                    if (insideA)
                        scratch[written++] = a;
                    if (insideA != insideB)
                        scratch[written++] = a + (b - a) * ((bound - da) / (db - da));
                }
                points = scratch;
                count  = written;
            }
            if (count < 3)
                return false;
            for (uint32_t i = 0; i < count; ++i)
                if (!Project(camera, points[i], extent, layout.points[i]))
                    return false;
            layout.count = count;
        }
        return true;
    }
    bool HitViewportPickProxy(
        const ViewportPickProxy &proxy, const ViewportPickRequest &request, float &distance, float &screenDistance, std::optional<Vector3> &position)
    {
        screenDistance = 0;
        position.reset();
        if (const auto *triangle = std::get_if<ViewportTriangleProxy>(&proxy.shape)) {
            const auto   ab = triangle->b - triangle->a, ac = triangle->c - triangle->a;
            const auto   p           = request.direction.Cross(ac);
            const double determinant = ab.Dot(p);
            if (determinant == 0)
                return false;
            const auto   offset = request.origin - triangle->a;
            const double u      = offset.Dot(p) / determinant;
            const auto   q      = offset.Cross(ab);
            const double v      = request.direction.Dot(q) / determinant;
            distance            = float(ac.Dot(q) / determinant);
            if (u < 0 || v < 0 || u + v > 1)
                return false;
            position = request.origin + request.direction * distance;
        } else {
            if (!request.view.available)
                return false;
            const auto         &view = request.view;
            ViewportProxyLayout layout;
            if (!ProjectViewportPickProxy(proxy, view.camera, view.extent, layout))
                return false;
            if (const auto *icon = std::get_if<ViewportBillboardProxy>(&proxy.shape)) {
                if (view.point.x < layout.points[0].x || view.point.y < layout.points[0].y || view.point.x > layout.points[2].x ||
                    view.point.y > layout.points[2].y)
                    return false;
                screenDistance = (view.point - (layout.points[0] + layout.points[2]) * .5f).Length();
                distance       = (icon->center - request.origin).Dot(request.direction);
            } else {
                const auto &line = std::get<ViewportSegmentProxy>(proxy.shape);
                auto        a = line.a, b = line.b;
                if (!ClipSegment(view.camera, a, b))
                    return false;
                Vector2 first, second;
                if (!Project(view.camera, a, view.extent, first) || !Project(view.camera, b, view.extent, second))
                    return false;
                const auto  delta  = second - first;
                const float square = delta.x * delta.x + delta.y * delta.y;
                const auto  offset = view.point - first;
                const float t      = square > 0 ? std::clamp((offset.x * delta.x + offset.y * delta.y) / square, 0.f, 1.f) : 0;
                screenDistance     = (view.point - (first + delta * t)).Length();
                if (screenDistance > line.width * .5f + line.tolerance)
                    return false;
                const float za = Depth(view.camera, a), zb = Depth(view.camera, b);
                const float perspectiveT = t * za / ((1 - t) * zb + t * za);
                position                 = a + (b - a) * perspectiveT;
                distance                 = (*position - request.origin).Dot(request.direction);
                if (distance < request.nearDistance && request.nearDistance - distance <= 1.e-5f)
                    distance = request.nearDistance;
            }
        }
        return std::isfinite(distance) && distance >= request.nearDistance && distance <= request.farDistance;
    }
} // namespace sky::editor
