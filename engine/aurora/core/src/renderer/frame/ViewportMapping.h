//
// Created on 2026/10/17.
//

#pragma once

#include <aurora/renderer/RendererTypes.h>
#include <aurora/rhi/SwapChain.h>
#include <core/math/Vector2.h>

namespace sky::aurora {
    struct ViewportMapping {
        Extent2D         imageExtent;
        SurfaceTransform transform = SurfaceTransform::IDENTITY;

        Extent2D LogicalExtent() const
        {
            return (static_cast<uint32_t>(transform) & 1) ? Extent2D{imageExtent.height, imageExtent.width} : imageExtent;
        }
        // Top-left normalized coordinates; mirror precedes clockwise rotation.
        Vector2 ToImage(Vector2 point) const
        {
            const auto value = static_cast<uint32_t>(transform);
            if (value >= 4)
                point.x = 1.f - point.x;
            switch (value % 4) {
            case 1: return {1.f - point.y, point.x};
            case 2: return {1.f - point.x, 1.f - point.y};
            case 3: return {point.y, 1.f - point.x};
            default: return point;
            }
        }
        Matrix4 ClipTransform() const
        {
            const auto origin = ToImage({0.5f, 0.5f});
            const auto x      = ToImage({1.f, 0.5f}) - origin;
            const auto y      = ToImage({0.5f, 1.f}) - origin;
            auto       result = RenderIdentityMatrix();
            result.v[0]       = x.x * 2.f;
            result.v[1]       = x.y * 2.f;
            result.v[4]       = y.x * 2.f;
            result.v[5]       = y.y * 2.f;
            return result;
        }
    };
} // namespace sky::aurora
