//
// Created on 2026/09/19.
//

#include <algorithm>
#include <array>
#include <cmath>
#include <ui/UIGeometry.h>
#include <ui/UIPaintContext.h>
#include <utility>

namespace sky::ui {

    void UIPaintContext::Begin(const UIRect &surfaceRect)
    {
        drawData.Clear();
        clipStack.clear();
        clipStack.push_back(surfaceRect);

        transform = UI2DTransform::Identity();
        transformStack.clear();
        opacity = 1.0f;
        opacityStack.clear();
    }

    void UIPaintContext::PushTransform(const UI2DTransform &local)
    {
        transformStack.push_back(transform);
        transform = transform * local;
    }

    void UIPaintContext::PopTransform()
    {
        if (!transformStack.empty()) {
            transform = transformStack.back();
            transformStack.pop_back();
        }
    }

    void UIPaintContext::PushOpacity(float local)
    {
        opacityStack.push_back(opacity);
        opacity *= local;
    }

    void UIPaintContext::PopOpacity()
    {
        if (!opacityStack.empty()) {
            opacity = opacityStack.back();
            opacityStack.pop_back();
        }
    }

    void UIPaintContext::PushClip(const UIRect &rect)
    {
        clipStack.push_back(UIRect::Intersect(clipStack.back(), TransformRect(rect, transform)));
    }

    void UIPaintContext::PopClip()
    {
        if (clipStack.size() > 1) {
            clipStack.pop_back();
        }
    }

    void UIPaintContext::AddRect(const UIRect &rect, uint32_t color)
    {
        AddQuad(rect, UIRect{}, UI_INVALID_TEXTURE, color, -1.0f);
    }

    void UIPaintContext::AddTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color)
    {
        transform.Apply(x0, y0);
        transform.Apply(x1, y1);
        transform.Apply(x2, y2);
        const auto &clip = clipStack.back();
        if (clip.IsEmpty())
            return;
        using Point = std::array<double, 2>;
        std::array<Point, 8> points{{{x0, y0}, {x1, y1}, {x2, y2}}};
        size_t               count = 3;
        // Scissors do not bound uploaded vertices. Clip in surface space before encoding the snapshot.
        if (std::isfinite(x0) && std::isfinite(y0) && std::isfinite(x1) && std::isfinite(y1) && std::isfinite(x2) && std::isfinite(y2)) {
            const std::array<double, 4> edges{clip.left, clip.right, clip.top, clip.bottom};
            for (size_t edge = 0; edge < edges.size() && count; ++edge) {
                std::array<Point, 8> clipped{};
                size_t               nextCount      = 0;
                const size_t         axis           = edge / 2;
                const double         bound          = edges[edge];
                const auto           inside         = [&](const Point &point) { return edge % 2 == 0 ? point[axis] >= bound : point[axis] <= bound; };
                auto                 previous       = points[count - 1];
                bool                 previousInside = inside(previous);
                for (size_t i = 0; i < count; ++i) {
                    const auto current       = points[i];
                    const bool currentInside = inside(current);
                    if (previousInside != currentInside) {
                        const double t = (bound - previous[axis]) / (current[axis] - previous[axis]);
                        Point        intersection{};
                        intersection[axis]     = bound;
                        intersection[1 - axis] = previous[1 - axis] + t * (current[1 - axis] - previous[1 - axis]);
                        clipped[nextCount++]   = intersection;
                    }
                    if (currentInside)
                        clipped[nextCount++] = current;
                    previous       = current;
                    previousInside = currentInside;
                }
                points = clipped;
                count  = nextCount;
            }
        }
        if (count < 3)
            return;
        const auto     base      = static_cast<uint32_t>(drawData.vertices.size());
        const auto     indexBase = static_cast<uint32_t>(drawData.indices.size());
        const uint32_t shaded    = ApplyOpacity(color);
        for (size_t i = 0; i < count; ++i) {
            UIVertex vertex{};
            vertex.x     = static_cast<float>(points[i][0]);
            vertex.y     = static_cast<float>(points[i][1]);
            vertex.color = shaded;
            drawData.vertices.push_back(vertex);
        }
        for (uint32_t i = 1; i + 1 < count; ++i)
            drawData.indices.insert(drawData.indices.end(), {base, base + i, base + i + 1});
        const auto indexCount = static_cast<uint32_t>((count - 2) * 3);
        if (!drawData.commands.empty()) {
            auto &last = drawData.commands.back();
            if (!last.shape && last.textureId == UI_INVALID_TEXTURE && last.clip.left == clip.left && last.clip.top == clip.top &&
                last.clip.right == clip.right && last.clip.bottom == clip.bottom) {
                last.indexCount += indexCount;
                return;
            }
        }
        UIDrawCmd command{};
        command.indexOffset = indexBase;
        command.indexCount  = indexCount;
        command.clip        = clip;
        drawData.commands.push_back(command);
    }

    void UIPaintContext::AddTexturedQuad(const UIRect &rect, const UIRect &uv, UITextureId textureId, uint32_t color)
    {
        AddQuad(rect, uv, textureId, color, -1.0f);
    }

    void UIPaintContext::AddRoundedRect(const UIRect &rect, uint32_t color, float radius)
    {
        AddQuad(rect, UIRect{}, UI_INVALID_TEXTURE, color, radius);
    }

    uint32_t UIPaintContext::ApplyOpacity(uint32_t color) const
    {
        if (opacity >= 1.0f) {
            return color;
        }
        const uint32_t alpha = (color >> 24) & 0xFF;
        const auto scaled = static_cast<uint32_t>(static_cast<float>(alpha) * opacity + 0.5f);
        return (color & 0x00FFFFFF) | (scaled << 24);
    }

    void UIPaintContext::AddQuad(const UIRect &rect, const UIRect &uv, UITextureId textureId, uint32_t color, float radius)
    {
        const UIRect &clip = clipStack.back();
        if (UIRect::Intersect(TransformRect(rect, transform), clip).IsEmpty()) {
            return;
        }

        const auto base      = static_cast<uint32_t>(drawData.vertices.size());
        const auto indexBase = static_cast<uint32_t>(drawData.indices.size());

        const uint32_t shaded = ApplyOpacity(color);
        const bool shape = radius >= 0.0f;
        const float centerX = (rect.left + rect.right) * 0.5f;
        const float centerY = (rect.top + rect.bottom) * 0.5f;
        const float halfX = (rect.right - rect.left) * 0.5f;
        const float halfY = (rect.bottom - rect.top) * 0.5f;
        // Shape shaders use untransformed layout coordinates for their SDF;
        // textured draws retain their normalized texture coordinates.
        const UIRect &drawUv = shape ? rect : uv;

        float x0 = rect.left;
        float y0 = rect.top;
        float x1 = rect.right;
        float y1 = rect.top;
        float x2 = rect.right;
        float y2 = rect.bottom;
        float x3 = rect.left;
        float y3 = rect.bottom;
        transform.Apply(x0, y0);
        transform.Apply(x1, y1);
        transform.Apply(x2, y2);
        transform.Apply(x3, y3);

        const auto makeVertex = [&](float x, float y, float u, float v) {
            UIVertex vert;
            vert.x = x;
            vert.y = y;
            vert.u = u;
            vert.v = v;
            vert.color = shaded;
            vert.roundCenterX = centerX;
            vert.roundCenterY = centerY;
            vert.roundHalfX = halfX;
            vert.roundHalfY = halfY;
            vert.roundRadius = radius < 0.0f ? 0.0f : radius;
            return vert;
        };
        drawData.vertices.push_back(makeVertex(x0, y0, drawUv.left, drawUv.top));
        drawData.vertices.push_back(makeVertex(x1, y1, drawUv.right, drawUv.top));
        drawData.vertices.push_back(makeVertex(x2, y2, drawUv.right, drawUv.bottom));
        drawData.vertices.push_back(makeVertex(x3, y3, drawUv.left, drawUv.bottom));

        drawData.indices.push_back(base + 0);
        drawData.indices.push_back(base + 1);
        drawData.indices.push_back(base + 2);
        drawData.indices.push_back(base + 0);
        drawData.indices.push_back(base + 2);
        drawData.indices.push_back(base + 3);

        if (!shape && !drawData.commands.empty()) {
            const UIDrawCmd &last = drawData.commands.back();
            const bool sameTexture = last.textureId == textureId;
            const bool sameClip = last.clip.left == clip.left && last.clip.top == clip.top &&
                                  last.clip.right == clip.right && last.clip.bottom == clip.bottom;
            if (sameTexture && sameClip && !last.shape) {
                drawData.commands.back().indexCount += 6;
                return;
            }
        }

        UIDrawCmd cmd;
        cmd.indexOffset = indexBase;
        cmd.indexCount  = 6;
        cmd.clip        = clip;
        cmd.textureId   = textureId;
        cmd.shape       = shape;
        drawData.commands.push_back(cmd);
    }

} // namespace sky::ui
