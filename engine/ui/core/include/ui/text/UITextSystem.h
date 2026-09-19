//
// Created on 2026/09/19.
//

#pragma once

#include <ui/text/UIFontAtlas.h>

#include <cstdint>

namespace sky::ui {

    class IUIFontProvider;
    class IUITextureRegistry;

    // Bundles the active font provider, texture registry, and glyph atlas.
    class UITextSystem {
    public:
        UITextSystem(IUIFontProvider *provider, IUITextureRegistry *registry, uint32_t pageSize = 256);

        UIFontAtlas &GetAtlas() { return atlas; }
        const UIFontAtlas &GetAtlas() const { return atlas; }
        IUIFontProvider *GetProvider() const { return provider; }

    private:
        IUIFontProvider *provider = nullptr;
        UIFontAtlas atlas;
    };

} // namespace sky::ui
