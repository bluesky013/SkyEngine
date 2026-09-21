//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/layout/LayoutNode.h>
#include <editor/core/layout/PanelRegistry.h>
#include <string>
#include <vector>

namespace sky::editor {

    // Toolkit- and render-independent editor layout.
    //
    // The model is a tree of split/tab nodes referencing panels by id; all
    // operations (split, move, tabify, close, set-ratio, reset) and JSON
    // persistence are headless. A frontend renders the computed areas.
    class LayoutModel {
    public:
        LayoutModel() = default;
        ~LayoutModel() = default;

        LayoutModel(const LayoutModel &) = delete;
        LayoutModel &operator=(const LayoutModel &) = delete;

        bool IsEmpty() const { return root == nullptr; }
        LayoutNode *GetRoot() const { return root.get(); }

        int GetVersion() const { return version; }

        // Replaces the layout with a single tab containing the given panels and
        // records it as the default arrangement.
        void SetDefault(std::vector<std::string> panelIds);
        void ResetToDefault();
        void Clear();

        // Splits the tab containing panelId, placing newPanelId on the trailing side.
        bool SplitPanel(const std::string &panelId, SplitOrientation orientation, const std::string &newPanelId);

        // Moves a panel into the tab that contains targetPanelId (same as tabify).
        bool MoveToArea(const std::string &panelId, const std::string &targetPanelId);
        bool Tabify(const std::string &panelId, const std::string &targetPanelId);

        // Removes a panel; empty tabs/splits collapse.
        bool ClosePanel(const std::string &panelId);

        // Sets the ratio at split position index (between child index and index+1).
        bool SetRatio(LayoutNode *splitNode, uint32_t index, float ratio);

        void CollectPanels(std::vector<std::string> &out) const;
        TabNode *FindTab(const std::string &panelId) const;
        LayoutNode *FindParent(const LayoutNode *node) const;

        // Versioned JSON persistence. When a registry is given, unknown panel ids
        // are skipped and reported through warnings.
        std::string ToJson(int indent = -1) const;
        static bool FromJson(const std::string &json, LayoutModel &out, const PanelRegistry *registry = nullptr,
                             std::vector<std::string> *warnings = nullptr);

    private:
        LayoutNodePtr root;
        std::vector<std::string> defaultPanels;
        bool hasDefault = false;
        int version = 1;
    };

} // namespace sky::editor
