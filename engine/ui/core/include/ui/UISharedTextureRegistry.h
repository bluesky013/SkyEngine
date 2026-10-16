#pragma once

#include <ui/IUITextureRegistry.h>

#include <unordered_map>
#include <vector>

namespace sky::ui {

    // CPU-owned shared images for one group of surfaces. Borrowed backend
    // registries attach/detach on the UI thread and outlive their attachment.
    // This catalog never sees GPU types or owns a backend registry.
    class UISharedTextureRegistry : public IUITextureRegistry {
    public:
        // Local/viewport ids use the low range; glyph pages start at 0x40000000.
        // Automatic shared ids use [AUTO_FIRST, AUTO_LAST]. Explicit shared ids
        // must be >= EXPLICIT_FIRST (e.g. the immutable editor color wheel).
        static constexpr UITextureId AUTO_FIRST     = 0x20000000u;
        static constexpr UITextureId AUTO_LAST      = 0x3FFFFFFFu;
        static constexpr UITextureId EXPLICIT_FIRST = 0x80000000u;

        void AddRegistry(IUITextureRegistry *registry);
        // Detachment makes no backend calls; already delivered textures remain
        // backend-owned until its safe shutdown. Detach before its destruction.
        void RemoveRegistry(IUITextureRegistry *registry);

        UITextureId RegisterTexture(const UIImageData &image) override;
        // Existing ids keep their stored image and are re-ensured on all live
        // registries, including a renderer whose texture table was reset.
        UITextureId RegisterTextureAs(UITextureId id, const UIImageData &image) override;
        void        UpdateTexture(UITextureId id, const UIImageData &image) override;
        void        ReleaseTexture(UITextureId id) override;

    private:
        UITextureId                                  Insert(UITextureId id, const UIImageData &image);
        std::unordered_map<UITextureId, UIImageData> images;
        std::vector<IUITextureRegistry *>            registries;
        UITextureId                                  nextTextureId = AUTO_FIRST;
    };

} // namespace sky::ui
