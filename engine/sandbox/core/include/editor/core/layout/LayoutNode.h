//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sky::editor {

    enum class LayoutNodeKind : uint8_t {
        SPLIT = 0,
        TAB,
    };

    enum class SplitOrientation : uint8_t {
        HORIZONTAL = 0,
        VERTICAL,
    };

    // Drop target position when docking a panel onto another panel.
    // CENTER tabifies; the four edges split on that side of the target.
    enum class DockPosition : uint8_t {
        CENTER = 0,
        LEFT,
        RIGHT,
        TOP,
        BOTTOM,
    };

    struct PanelNode {
        std::string panelId;
    };

    // A panel presented in its own OS window. Geometry is in device pixels.
    struct FloatingPanel {
        std::string panelId;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        bool active = false;
    };

    struct LayoutNode {
        explicit LayoutNode(LayoutNodeKind kind) : kind(kind) {}
        virtual ~LayoutNode() = default;

        LayoutNode(const LayoutNode &) = delete;
        LayoutNode &operator=(const LayoutNode &) = delete;

        LayoutNodeKind kind;
    };

    struct TabNode : public LayoutNode {
        TabNode() : LayoutNode(LayoutNodeKind::TAB) {}

        std::vector<PanelNode> panels;
        int32_t activeIndex = 0;
    };

    struct SplitNode : public LayoutNode {
        SplitNode() : LayoutNode(LayoutNodeKind::SPLIT) {}

        SplitOrientation orientation = SplitOrientation::HORIZONTAL;
        std::vector<float> ratios;
        std::vector<std::unique_ptr<LayoutNode>> children;
    };

    using LayoutNodePtr = std::unique_ptr<LayoutNode>;

    inline bool IsSplit(const LayoutNode *node) { return node != nullptr && node->kind == LayoutNodeKind::SPLIT; }
    inline bool IsTab(const LayoutNode *node) { return node != nullptr && node->kind == LayoutNodeKind::TAB; }

} // namespace sky::editor
