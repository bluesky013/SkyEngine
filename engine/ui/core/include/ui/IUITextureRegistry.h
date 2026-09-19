//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIDrawData.h>

#include <cstdint>
#include <vector>

namespace sky::ui {

    struct UIImageData {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels; // RGBA8
    };

    // Core-side seam: the render module turns UI images (atlas pages, icons) into
    // opaque texture handles referenced by UIDrawCmd. Core never sees GPU types.
    class IUITextureRegistry {
    public:
        IUITextureRegistry() = default;
        virtual ~IUITextureRegistry() = default;

        IUITextureRegistry(const IUITextureRegistry &) = delete;
        IUITextureRegistry &operator=(const IUITextureRegistry &) = delete;

        virtual UITextureId RegisterTexture(const UIImageData &image) = 0;
        virtual void UpdateTexture(UITextureId id, const UIImageData &image) = 0;
        virtual void ReleaseTexture(UITextureId id) = 0;
    };

} // namespace sky::ui
