//
// Created on 2026/10/08.
//

#include <editor/core/asset/EditorAssetEditor.h>

#include <algorithm>

namespace sky::editor {

    void EditorAssetEditorRegistry::Register(IEditorAssetEditor *editor)
    {
        if (editor == nullptr) {
            return;
        }
        if (std::find(editors.begin(), editors.end(), editor) == editors.end()) {
            editors.push_back(editor);
        }
    }

    void EditorAssetEditorRegistry::Unregister(IEditorAssetEditor *editor)
    {
        editors.erase(std::remove(editors.begin(), editors.end(), editor), editors.end());
    }

    IEditorAssetEditor *EditorAssetEditorRegistry::Find(const std::string &type) const
    {
        for (auto *editor : editors) {
            if (editor != nullptr && editor->GetAssetType() == type) {
                return editor;
            }
        }
        return nullptr;
    }

} // namespace sky::editor
