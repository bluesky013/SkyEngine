//
// Created on 2026/09/19.
//

#include <ui/widgets/Image.h>
#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::ui {

    void Image::SetNineSlice(float left, float top, float right, float bottom)
    {
        sliceLeft = left;
        sliceTop = top;
        sliceRight = right;
        sliceBottom = bottom;
        MarkPaintDirty();
    }

    void Image::ClearNineSlice()
    {
        sliceLeft = 0.0f;
        sliceTop = 0.0f;
        sliceRight = 0.0f;
        sliceBottom = 0.0f;
        MarkPaintDirty();
    }

    void Image::OnPaint(UIPaintContext &context)
    {
        if (texture == UI_INVALID_TEXTURE) {
            return;
        }

        const UIRect &rect = GetBounds();
        if (!HasNineSlice()) {
            context.AddTexturedQuad(rect, uv, texture, tint);
            return;
        }

        const float width = rect.Width();
        const float height = rect.Height();

        // Clamp insets so opposite slices never overlap a small rect.
        const float left = std::min(sliceLeft, width * 0.5f);
        const float right = std::min(sliceRight, width * 0.5f);
        const float top = std::min(sliceTop, height * 0.5f);
        const float bottom = std::min(sliceBottom, height * 0.5f);

        // UV insets derive from the destination ratio (source size is unknown here).
        const float uLeft = width > 0.0f ? uv.Width() * (left / width) : 0.0f;
        const float uRight = width > 0.0f ? uv.Width() * (right / width) : 0.0f;
        const float vTop = height > 0.0f ? uv.Height() * (top / height) : 0.0f;
        const float vBottom = height > 0.0f ? uv.Height() * (bottom / height) : 0.0f;

        const float xs[4] = {rect.left, rect.left + left, rect.right - right, rect.right};
        const float ys[4] = {rect.top, rect.top + top, rect.bottom - bottom, rect.bottom};
        const float us[4] = {uv.left, uv.left + uLeft, uv.right - uRight, uv.right};
        const float vs[4] = {uv.top, uv.top + vTop, uv.bottom - vBottom, uv.bottom};

        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                UIRect cell;
                cell.left = xs[col];
                cell.top = ys[row];
                cell.right = xs[col + 1];
                cell.bottom = ys[row + 1];

                UIRect cellUv;
                cellUv.left = us[col];
                cellUv.top = vs[row];
                cellUv.right = us[col + 1];
                cellUv.bottom = vs[row + 1];

                context.AddTexturedQuad(cell, cellUv, texture, tint);
            }
        }
    }

    bool Image::SetProperty(const std::string &path, const UIPropertyValue &value)
    {
        if (path == "visual.texture" && value.type == UIPropertyValue::Type::INT) {
            SetTexture(static_cast<UITextureId>(value.intValue));
            return true;
        }
        if (path == "visual.tint") {
            if (value.type == UIPropertyValue::Type::INT) {
                SetTint(static_cast<uint32_t>(value.intValue));
                return true;
            }
            if (value.type == UIPropertyValue::Type::FLOAT) {
                SetTint(static_cast<uint32_t>(value.floatValue));
                return true;
            }
        }
        return UIElement::SetProperty(path, value);
    }

} // namespace sky::ui
