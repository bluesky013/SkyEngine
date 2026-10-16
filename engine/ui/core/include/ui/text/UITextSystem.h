//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UISharedTextureRegistry.h>
#include <ui/text/UIFontAtlas.h>

#include <cstdint>

namespace sky::ui {

    class IUIFontProvider;
    class IUITextureRegistry;

    // Bundles the active font provider, texture registry, and glyph atlas.
    class UITextSystem {
    public:
        UITextSystem(IUIFontProvider *provider, IUITextureRegistry *registry, uint32_t pageSize = 256);

        UIFontAtlas &GetAtlas()
        {
            return atlas;
        }
        const UIFontAtlas &GetAtlas() const
        {
            return atlas;
        }
        IUIFontProvider *GetProvider() const
        {
            return provider;
        }
        IUITextureRegistry *GetRegistry() const
        {
            return registry;
        }
        // Shared CPU images (icons/color wheel) replay and update every attached
        // surface. GetRegistry above remains the borrowed default backend.
        IUITextureRegistry *GetSharedRegistry()
        {
            return &sharedTextures;
        }

        // Register the atlas pages into an additional texture registry (a second
        // window's renderer) so text renders in every surface.
        void AddRegistry(IUITextureRegistry *value)
        {
            atlas.AddRegistry(value);
            sharedTextures.AddRegistry(value);
            if (registry == nullptr) {
                registry = value;
            }
        }
        // The caller detaches before destroying a borrowed registry. Its textures
        // remain owned by that registry until its GPU-safe shutdown.
        void RemoveRegistry(IUITextureRegistry *value)
        {
            atlas.RemoveRegistry(value);
            sharedTextures.RemoveRegistry(value);
            if (registry == value) {
                registry = nullptr;
            }
        }

    private:
        IUIFontProvider        *provider = nullptr;
        IUITextureRegistry     *registry = nullptr;
        UIFontAtlas             atlas;
        UISharedTextureRegistry sharedTextures;
    };

} // namespace sky::ui
