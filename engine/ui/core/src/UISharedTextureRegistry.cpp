#include <ui/UISharedTextureRegistry.h>

#include <algorithm>
#include <limits>

namespace sky::ui {
    namespace {
        bool ValidImage(const UIImageData &image)
        {
            if (image.width == 0 || image.height == 0 || image.width > std::numeric_limits<size_t>::max() / 4 / image.height) {
                return false;
            }
            return image.pixels.size() == static_cast<size_t>(image.width) * image.height * 4;
        }
    } // namespace

    void UISharedTextureRegistry::AddRegistry(IUITextureRegistry *registry)
    {
        if (registry == nullptr || registry == this || std::find(registries.begin(), registries.end(), registry) != registries.end()) {
            return;
        }
        registries.push_back(registry);
        for (const auto &[id, image] : images) {
            registry->RegisterTextureAs(id, image);
            // Reattaching the same backend can retain an older generation under
            // this id. Replay current pixels as well as ensuring its presence.
            registry->UpdateTexture(id, image);
        }
    }

    void UISharedTextureRegistry::RemoveRegistry(IUITextureRegistry *registry)
    {
        registries.erase(std::remove(registries.begin(), registries.end(), registry), registries.end());
    }

    UITextureId UISharedTextureRegistry::Insert(UITextureId id, const UIImageData &image)
    {
        if (!ValidImage(image)) {
            return UI_INVALID_TEXTURE;
        }
        images.emplace(id, image);
        for (auto *registry : registries) {
            registry->RegisterTextureAs(id, image);
        }
        return id;
    }

    UITextureId UISharedTextureRegistry::RegisterTexture(const UIImageData &image)
    {
        if (nextTextureId > AUTO_LAST) {
            return UI_INVALID_TEXTURE;
        }
        const UITextureId id = Insert(nextTextureId, image);
        if (id != UI_INVALID_TEXTURE) {
            ++nextTextureId;
        }
        return id;
    }

    UITextureId UISharedTextureRegistry::RegisterTextureAs(UITextureId id, const UIImageData &image)
    {
        if (id < EXPLICIT_FIRST) {
            return UI_INVALID_TEXTURE;
        }
        const auto existing = images.find(id);
        if (existing == images.end()) {
            return Insert(id, image);
        }
        for (auto *registry : registries) {
            registry->RegisterTextureAs(id, existing->second);
        }
        return id;
    }

    void UISharedTextureRegistry::UpdateTexture(UITextureId id, const UIImageData &image)
    {
        const auto existing = images.find(id);
        if (existing == images.end() || !ValidImage(image)) {
            return;
        }
        existing->second = image;
        for (auto *registry : registries) {
            registry->UpdateTexture(id, image);
        }
    }

    void UISharedTextureRegistry::ReleaseTexture(UITextureId id)
    {
        if (images.erase(id) == 0) {
            return;
        }
        for (auto *registry : registries) {
            registry->ReleaseTexture(id);
        }
    }

} // namespace sky::ui
