//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/layout/LayoutNode.h>
#include <cstdint>
#include <vector>

namespace sky::editor {

    // Axis-aligned rectangle in device pixels. Kept UI-toolkit-free so all
    // docking/splitter geometry is reusable and headless-testable.
    struct LayoutRect {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;

        float Width() const { return right - left; }
        float Height() const { return bottom - top; }
        bool Contains(float x, float y) const { return x >= left && x < right && y >= top && y < bottom; }
    };

    // One draggable splitter band between two adjacent children of a split.
    // `orientation` is the split axis; the band is a thin strip perpendicular
    // to it, centered on the seam.
    struct SplitterBand {
        SplitNode        *split = nullptr;
        uint32_t          index = 0; // ratio index (between child index and index+1)
        SplitOrientation  orientation = SplitOrientation::HORIZONTAL;
        LayoutRect        rect;       // the hit area in device pixels
        LayoutRect        parentRect; // the owning split's rect (for ratio math)
    };

    // Computes the child rectangles of a split node within rect, applying the
    // split's ratios (equal weights when the ratio count does not match).
    void ComputeChildRects(const SplitNode &split, const LayoutRect &rect, std::vector<LayoutRect> &out);

    // Collects draggable splitter bands for a laid-out node subtree. thickness
    // is the band hit width in device pixels.
    void CollectSplitterBands(LayoutNode *node, const LayoutRect &rect, float thickness,
                              std::vector<SplitterBand> &out);

    // New ratio for a splitter band given the pointer position along its axis.
    float RatioFromDrag(const SplitterBand &band, float pointerX, float pointerY);

    // Resolves a pointer inside a tab body to a dock position: the central
    // region (1 - 2*edgeFraction on each axis) is CENTER, and a pointer within
    // edgeFraction of an edge returns that edge.
    DockPosition ResolveDockPosition(const LayoutRect &rect, float x, float y, float edgeFraction);

} // namespace sky::editor
