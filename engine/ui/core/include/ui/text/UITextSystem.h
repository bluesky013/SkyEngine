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
        IUITextureRegistry *GetRegistry() const { return registry; }

        // Register the atlas pages into an additional texture registry (a second
        // window's renderer) so text renders in every surface.
        void AddRegistry(IUITextureRegistry *value) { atlas.AddRegistry(value); }

    private:
        IUIFontProvider *provider = nullptr;
        IUITextureRegistry *registry = nullptr;
        UIFontAtlas atlas;
    };

} // namespace sky::ui
