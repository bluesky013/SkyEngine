//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIDrawData.h>

#include <vector>

namespace sky::ui {

    class UITheme;

    class UIPaintContext {
    public:
        // Resets draw data and seeds the clip stack with the surface rect.
        void Begin(const UIRect &surfaceRect);

        void PushClip(const UIRect &rect);
        void PopClip();

        void PushTransform(const UI2DTransform &local);
        void PopTransform();
        void PushOpacity(float local);
        void PopOpacity();

        void AddRect(const UIRect &rect, uint32_t color);
        void AddTexturedQuad(const UIRect &rect, const UIRect &uv, UITextureId textureId, uint32_t color);

        const UIDrawData &GetDrawData() const { return drawData; }
        const UIRect &CurrentClip() const { return clipStack.back(); }

        void SetTheme(const UITheme *value) { theme = value; }
        const UITheme *GetTheme() const { return theme; }

    private:
        void AddQuad(const UIRect &rect, const UIRect &uv, UITextureId textureId, uint32_t color);
        uint32_t ApplyOpacity(uint32_t color) const;

        UIDrawData drawData;
        std::vector<UIRect> clipStack;
        UI2DTransform transform;
        std::vector<UI2DTransform> transformStack;
        float opacity = 1.0f;
        std::vector<float> opacityStack;
        const UITheme *theme = nullptr;
    };

} // namespace sky::ui
