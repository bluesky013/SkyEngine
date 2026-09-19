//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIDrawData.h>

namespace sky::ui {

    class Image : public UIElement {
    public:
        Image() = default;
        ~Image() override = default;

        const char *GetTypeName() const override { return "Image"; }

        void SetTexture(UITextureId value)
        {
            texture = value;
            MarkPaintDirty();
        }
        UITextureId GetTexture() const { return texture; }

        void SetUv(const UIRect &value)
        {
            uv = value;
            MarkPaintDirty();
        }
        const UIRect &GetUv() const { return uv; }

        void SetTint(uint32_t value)
        {
            tint = value;
            MarkPaintDirty();
        }
        uint32_t GetTint() const { return tint; }

        void SetNineSlice(float left, float top, float right, float bottom);
        void ClearNineSlice();
        bool HasNineSlice() const
        {
            return sliceLeft > 0.0f || sliceTop > 0.0f || sliceRight > 0.0f || sliceBottom > 0.0f;
        }

        void OnPaint(UIPaintContext &context) override;
        bool SetProperty(const std::string &path, const UIPropertyValue &value) override;

    private:
        UITextureId texture = UI_INVALID_TEXTURE;
        UIRect uv{0.0f, 0.0f, 1.0f, 1.0f};
        uint32_t tint = 0xFFFFFFFF;
        float sliceLeft = 0.0f;
        float sliceTop = 0.0f;
        float sliceRight = 0.0f;
        float sliceBottom = 0.0f;
    };

} // namespace sky::ui
