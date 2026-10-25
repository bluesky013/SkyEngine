//
// Per-asset-type customization seam for the asset viewer (see asset-viewer-widget). A provider keyed by
// asset type can supply a custom content widget (right region; default = reflected cook settings). The
// left preview region is reserved and served by the separate IAssetPreviewProvider seam (editor/core).
// Widgets are `UIElement`s, so providers live in the UI layer (EditorUi).
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>
#include <ui/UIElement.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class CommandService;
    class IAssetViewDocument {
    public:
        virtual ~IAssetViewDocument()                                                          = default;
        virtual void SetActionScope(std::string) {}
        virtual std::unique_ptr<sky::ui::UIElement> CreateContent(sky::ui::UITextSystem *text) = 0;
        virtual std::unique_ptr<sky::ui::UIElement> CreatePreview(sky::ui::UITextSystem *text) { return nullptr; }
        virtual bool                                FitsContentToViewport() const
        {
            return false;
        }
        virtual CommandService                     &History()                                  = 0;
        virtual bool                                IsDirty() const                            = 0;
        virtual bool                                IsEditing() const                          = 0;
        virtual void                                CancelInteractions()                       = 0;
        virtual void                                RequestSave()                              = 0;
        virtual bool                                IsSavePending() const                      = 0;
        virtual std::string                         Status() const                             = 0;
        // Called once per host tick, outside UI dispatch. Content dies before this document.
        virtual void Tick(float delta) = 0;
    };
    struct AssetViewDocument {
        AssetViewDocument() = default;
        AssetViewDocument(std::shared_ptr<const void> code, std::unique_ptr<IAssetViewDocument> value)
            : moduleLease(std::move(code)), document(std::move(value)) {}
        AssetViewDocument(AssetViewDocument &&) noexcept = default;
        AssetViewDocument &operator=(AssetViewDocument other) noexcept
        {
            moduleLease.swap(other.moduleLease);
            document.swap(other.document);
            return *this;
        }
        std::shared_ptr<const void>         moduleLease;
        std::unique_ptr<IAssetViewDocument> document;
    };

    class IAssetViewProvider {
    public:
        virtual ~IAssetViewProvider() = default;
        virtual bool UsesDocumentWindow() const { return false; }

        virtual AssetViewDocument OpenDocument(const Uuid &uuid, const std::string &type)
        {
            return {};
        }

        // Content for the right region; null -> the default reflected cook-settings form.
        // `text` is the host text system (a content widget draws its own labels/fields).
        virtual std::unique_ptr<sky::ui::UIElement> CreateContent(const Uuid &uuid, const std::string &type, sky::ui::UITextSystem *text)
        {
            (void)uuid;
            (void)type;
            (void)text;
            return nullptr;
        }

        // Flush the content's buffered edits to the source and re-cook (the asset-view base window
        // owns the Save/close action). `content` is the element returned by CreateContent. Returns
        // true when something was committed.
        virtual bool SaveContent(const Uuid &uuid, const std::string &type, sky::ui::UIElement *content)
        {
            (void)uuid;
            (void)type;
            (void)content;
            return false;
        }
    };

    // Process-wide (cross-DLL) registry keyed by asset type.
    class AssetViewProviderRegistry : public sky::Singleton<AssetViewProviderRegistry> {
        friend class sky::Singleton<AssetViewProviderRegistry>;

    public:
        void Register(const std::string &type, IAssetViewProvider *provider, std::shared_ptr<const void> lease = {})
        {
            providers[type] = {std::move(lease), provider};
        }
        void Unregister(const std::string &type, IAssetViewProvider *expected = nullptr)
        {
            const auto found = providers.find(type);
            if (found != providers.end() && (!expected || expected == found->second.provider)) providers.erase(found);
        }
        IAssetViewProvider *Find(const std::string &type) const
        {
            const auto it = providers.find(type);
            return it != providers.end() ? it->second.provider : nullptr;
        }
        std::shared_ptr<const void> Retain(const std::string &type) const
        {
            const auto found = providers.find(type);
            return found == providers.end() ? nullptr : found->second.moduleLease;
        }

    private:
        struct Entry {
            std::shared_ptr<const void> moduleLease;
            IAssetViewProvider *provider = nullptr;
        };
        std::unordered_map<std::string, Entry> providers;
    };

} // namespace sky::editor
