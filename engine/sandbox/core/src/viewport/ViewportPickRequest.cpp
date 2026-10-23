//
// Created on 2026/10/23.
//

#include <editor/core/viewport/IViewportPicker.h>

#include <cmath>

namespace sky::editor {
    bool MakeViewportPickRequest(const EditorCamera  &camera,
                                 Vector2              point,
                                 Vector2              extent,
                                 uint32_t             imageWidth,
                                 uint32_t             imageHeight,
                                 ViewportPickContext  context,
                                 ViewportPickRequest &result)
    {
        if (!std::isfinite(camera.position.x) || !std::isfinite(camera.position.y) || !std::isfinite(camera.position.z) ||
            !std::isfinite(camera.verticalFov) || camera.verticalFov <= 0 || camera.verticalFov >= 3.14159265f || camera.nearPlane <= 0)
            return false;
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(extent.x) || !std::isfinite(extent.y) || extent.x <= 0 ||
            extent.y <= 0 || point.x < 0 || point.y < 0 || point.x >= extent.x || point.y >= extent.y || !imageWidth || !imageHeight)
            return false;
        result              = {};
        result.context      = context;
        result.view         = {camera,
                               point,
                               extent,
                               imageWidth,
                               imageHeight,
                               uint32_t(std::floor(double(point.x) * imageWidth / extent.x)),
                               uint32_t(std::floor(double(point.y) * imageHeight / extent.y)),
                               true};
        result.origin       = camera.position;
        result.direction    = camera.Ray(point, extent);
        const float forward = result.direction.Dot(camera.Forward());
        if (!std::isfinite(forward) || forward <= 0)
            return false;
        result.nearDistance = camera.nearPlane / forward;
        result.farDistance  = camera.farPlane / forward;
        return std::isfinite(result.nearDistance) && std::isfinite(result.farDistance) && result.nearDistance >= 0 &&
               result.farDistance >= result.nearDistance;
    }

    bool SameViewportPickRequest(const ViewportPickRequest &a, const ViewportPickRequest &b)
    {
        const auto  same3 = [](Vector3 x, Vector3 y) { return x.x == y.x && x.y == y.y && x.z == y.z; };
        const auto  same2 = [](Vector2 x, Vector2 y) { return x.x == y.x && x.y == y.y; };
        const auto &x     = a.view;
        const auto &y     = b.view;
        return a.context == b.context && same3(a.origin, b.origin) && same3(a.direction, b.direction) && a.nearDistance == b.nearDistance &&
               a.farDistance == b.farDistance && a.policy == b.policy && a.visibilityMask == b.visibilityMask && a.include == b.include &&
               a.ignore == b.ignore && a.locked == b.locked && x.available == y.available && same2(x.point, y.point) && same2(x.extent, y.extent) &&
               x.imageWidth == y.imageWidth && x.imageHeight == y.imageHeight && x.pixelX == y.pixelX && x.pixelY == y.pixelY &&
               same3(x.camera.position, y.camera.position) && x.camera.yaw == y.camera.yaw && x.camera.pitch == y.camera.pitch &&
               x.camera.verticalFov == y.camera.verticalFov && x.camera.nearPlane == y.camera.nearPlane && x.camera.farPlane == y.camera.farPlane;
    }
} // namespace sky::editor
