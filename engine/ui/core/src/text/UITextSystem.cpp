//
// Created on 2026/09/19.
//

#include <ui/text/UITextSystem.h>

namespace sky::ui {

    UITextSystem::UITextSystem(IUIFontProvider *provider, IUITextureRegistry *registry, uint32_t pageSize)
        : provider(provider)
        , atlas(registry, pageSize)
    {
        atlas.SetProvider(provider);
    }

} // namespace sky::ui
