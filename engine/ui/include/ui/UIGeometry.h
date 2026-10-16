// Layout geometry and pure mappings to the surface used by paint and input.

#pragma once

#include <ui/UIRect.h>

namespace sky::ui {

    // Scissor clips use the surface-space AABB of all four transformed corners.
    // Empty or non-finite geometry contributes no visible area.
    inline UIRect TransformRect(const UIRect &rect, const UI2DTransform &matrix)
    {
        if (rect.IsEmpty()) {
            return {};
        }
        UIPoint corners[] = {{rect.left, rect.top}, {rect.right, rect.top}, {rect.right, rect.bottom}, {rect.left, rect.bottom}};
        for (auto &corner : corners) {
            matrix.Apply(corner.x, corner.y);
            if (!std::isfinite(corner.x) || !std::isfinite(corner.y)) {
                return {};
            }
        }

        UIRect surface{corners[0].x, corners[0].y, corners[0].x, corners[0].y};
        for (const auto &corner : corners) {
            surface.left = std::min(surface.left, corner.x);
            surface.top = std::min(surface.top, corner.y);
            surface.right = std::max(surface.right, corner.x);
            surface.bottom = std::max(surface.bottom, corner.y);
        }
        return surface;
    }

    struct UIGeometry {
        UIRect layoutBounds;
        UI2DTransform layoutToSurface;

        UIRect SurfaceBounds() const
        {
            return TransformRect(layoutBounds, layoutToSurface);
        }

        bool SurfaceToLayout(float x, float y, UIPoint &out) const
        {
            if (!std::isfinite(x) || !std::isfinite(y)) {
                return false;
            }
            UI2DTransform inverse;
            if (!layoutToSurface.TryInverse(inverse)) {
                return false;
            }
            const double layoutX = static_cast<double>(inverse.m00) * x + static_cast<double>(inverse.m01) * y + inverse.m02;
            const double layoutY = static_cast<double>(inverse.m10) * x + static_cast<double>(inverse.m11) * y + inverse.m12;
            constexpr double limit = std::numeric_limits<float>::max();
            if (!std::isfinite(layoutX) || !std::isfinite(layoutY) || layoutX < -limit || layoutX > limit || layoutY < -limit || layoutY > limit) {
                return false;
            }
            out = {static_cast<float>(layoutX), static_cast<float>(layoutY)};
            return true;
        }

        bool ContainsSurfacePoint(float x, float y) const
        {
            UIPoint layoutPoint;
            return SurfaceToLayout(x, y, layoutPoint) && layoutBounds.Contains(layoutPoint.x, layoutPoint.y);
        }
    };

} // namespace sky::ui
