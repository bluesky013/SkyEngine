//
// Created on 2026/10/08.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>

#include <string>
#include <vector>

namespace sky::editor {

    // Toolkit-independent asset editor. An extension registers one per asset type so the browser can
    // open an asset. Opening is expected to load the asset's data through the product-based loader
    // (on-demand cook), never from source files.
    class IEditorAssetEditor {
    public:
        virtual ~IEditorAssetEditor() = default;

        virtual std::string GetAssetType() const   = 0;
        virtual void        Open(const Uuid &uuid) = 0;
    };

    // Process-wide (cross-DLL) registry of asset editors keyed by asset type id.
    class EditorAssetEditorRegistry : public sky::Singleton<EditorAssetEditorRegistry> {
        friend class sky::Singleton<EditorAssetEditorRegistry>;

    public:
        void Register(IEditorAssetEditor *editor);
        void Unregister(IEditorAssetEditor *editor);

        // First editor registered for the type, or nullptr.
        IEditorAssetEditor *Find(const std::string &type) const;

    private:
        std::vector<IEditorAssetEditor *> editors;
    };

} // namespace sky::editor
