//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/data/UIElementRegistry.h>

#include <string>
#include <vector>

namespace sky::ui {

    // Headless UI document editing model: mutate the tree, serialize to the UI
    // document format, and undo/redo via snapshots. A visual shell is a separate
    // concern that depends on the render pass.
    class UIDocumentEditor {
    public:
        UIDocumentEditor();

        UIElement *GetRoot() const { return root.get(); }
        UIElement *Find(const std::string &name) const;

        // parentName empty means the root. Returns the new element or null.
        UIElement *AddChild(const std::string &parentName, const std::string &type);
        bool Remove(const std::string &name);
        bool Rename(const std::string &name, const std::string &newName);
        bool MoveUp(const std::string &name);
        bool MoveDown(const std::string &name);

        std::string Serialize() const;
        bool Deserialize(const std::string &json);

        bool CanUndo() const { return !undoStack.empty(); }
        bool CanRedo() const { return !redoStack.empty(); }
        bool Undo();
        bool Redo();

    private:
        void Snapshot();

        UIElementRegistry registry;
        UIElementPtr root;
        std::vector<std::string> undoStack;
        std::vector<std::string> redoStack;
    };

} // namespace sky::ui
