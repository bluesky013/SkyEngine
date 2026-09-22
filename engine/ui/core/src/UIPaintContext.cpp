//
// Created on 2026/09/19.
//

#include <ui/UIPaintContext.h>

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
        clipStack.push_back(UIRect::Intersect(clipStack.back(), rect));
    }

    void UIPaintContext::PopClip()
    {
        if (clipStack.size() > 1) {
            clipStack.pop_back();
        }
    }

    void UIPaintContext::AddRect(const UIRect &rect, uint32_t color)
    {
        AddQuad(rect, UIRect{}, UI_INVALID_TEXTURE, color);
    }

    void UIPaintContext::AddTexturedQuad(const UIRect &rect, const UIRect &uv, UITextureId textureId, uint32_t color)
    {
        AddQuad(rect, uv, textureId, color);
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

    void UIPaintContext::AddQuad(const UIRect &rect, const UIRect &uv, UITextureId textureId, uint32_t color)
    {
        const UIRect &clip = clipStack.back();
        if (UIRect::Intersect(rect, clip).IsEmpty()) {
            return;
        }

        const auto base      = static_cast<uint32_t>(drawData.vertices.size());
        const auto indexBase = static_cast<uint32_t>(drawData.indices.size());

        const uint32_t shaded = ApplyOpacity(color);
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

        drawData.vertices.push_back({x0, y0, uv.left, uv.top, shaded});
        drawData.vertices.push_back({x1, y1, uv.right, uv.top, shaded});
        drawData.vertices.push_back({x2, y2, uv.right, uv.bottom, shaded});
        drawData.vertices.push_back({x3, y3, uv.left, uv.bottom, shaded});

        drawData.indices.push_back(base + 0);
        drawData.indices.push_back(base + 1);
        drawData.indices.push_back(base + 2);
        drawData.indices.push_back(base + 0);
        drawData.indices.push_back(base + 2);
        drawData.indices.push_back(base + 3);

        if (!drawData.commands.empty()) {
            const UIDrawCmd &last = drawData.commands.back();
            const bool sameTexture = last.textureId == textureId;
            const bool sameClip = last.clip.left == clip.left && last.clip.top == clip.top &&
                                  last.clip.right == clip.right && last.clip.bottom == clip.bottom;
            if (sameTexture && sameClip) {
                drawData.commands.back().indexCount += 6;
                return;
            }
        }

        UIDrawCmd cmd;
        cmd.indexOffset = indexBase;
        cmd.indexCount  = 6;
        cmd.clip        = clip;
        cmd.textureId   = textureId;
        drawData.commands.push_back(cmd);
    }

} // namespace sky::ui
