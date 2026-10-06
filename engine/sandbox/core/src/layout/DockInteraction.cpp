//
// Created on 2026/10/06.
//

#include <editor/core/layout/DockInteraction.h>

#include <algorithm>

namespace sky::editor {

    void ComputeChildRects(const SplitNode &split, const LayoutRect &rect, std::vector<LayoutRect> &out)
    {
        out.clear();
        const uint32_t count = static_cast<uint32_t>(split.children.size());
        if (count == 0) {
            return;
        }

        std::vector<float> weights(count, 1.0f);
        if (split.ratios.size() == count) {
            weights = split.ratios;
        }
        float total = 0.0f;
        for (float weight : weights) {
            total += std::max(weight, 0.0f);
        }
        if (total <= 0.0f) {
            std::fill(weights.begin(), weights.end(), 1.0f);
            total = static_cast<float>(count);
        }

        const bool horizontal = split.orientation == SplitOrientation::HORIZONTAL;
        const float span = horizontal ? rect.Width() : rect.Height();
        float cursor = horizontal ? rect.left : rect.top;

        out.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const float extent = span * (weights[i] / total);
            LayoutRect slot = rect;
            if (horizontal) {
                slot.left  = cursor;
                slot.right = (i + 1 == count) ? rect.right : cursor + extent;
                cursor     = slot.right;
            } else {
                slot.top    = cursor;
                slot.bottom = (i + 1 == count) ? rect.bottom : cursor + extent;
                cursor      = slot.bottom;
            }
            out.push_back(slot);
        }
    }

    namespace {

        void CollectBandsImpl(LayoutNode *node, const LayoutRect &rect, float thickness,
                              std::vector<SplitterBand> &out)
        {
            if (node == nullptr || !IsSplit(node)) {
                return;
            }
            auto *split = static_cast<SplitNode *>(node);
            std::vector<LayoutRect> children;
            ComputeChildRects(*split, rect, children);

            const bool horizontal = split->orientation == SplitOrientation::HORIZONTAL;
            const float half = thickness * 0.5f;
            for (uint32_t i = 0; i + 1 < static_cast<uint32_t>(children.size()); ++i) {
                SplitterBand band;
                band.split       = split;
                band.index       = i;
                band.orientation = split->orientation;
                band.parentRect  = rect;
                const float seam = horizontal ? children[i].right : children[i].bottom;
                if (horizontal) {
                    band.rect = LayoutRect{seam - half, rect.top, seam + half, rect.bottom};
                } else {
                    band.rect = LayoutRect{rect.left, seam - half, rect.right, seam + half};
                }
                out.push_back(band);
            }

            for (size_t i = 0; i < split->children.size(); ++i) {
                if (i < children.size()) {
                    CollectBandsImpl(split->children[i].get(), children[i], thickness, out);
                }
            }
        }

    } // namespace

    void CollectSplitterBands(LayoutNode *node, const LayoutRect &rect, float thickness,
                              std::vector<SplitterBand> &out)
    {
        CollectBandsImpl(node, rect, thickness, out);
    }

    float RatioFromDrag(const SplitterBand &band, float pointerX, float pointerY)
    {
        const float extent = band.orientation == SplitOrientation::HORIZONTAL ? band.parentRect.Width()
                                                                             : band.parentRect.Height();
        if (extent <= 0.0f) {
            return 0.5f;
        }
        const float start = band.orientation == SplitOrientation::HORIZONTAL ? band.parentRect.left
                                                                            : band.parentRect.top;
        const float pointer = band.orientation == SplitOrientation::HORIZONTAL ? pointerX : pointerY;
        const float fraction = (pointer - start) / extent;

        const auto &ratios = band.split->ratios;
        const size_t count = band.split->children.size();
        float before = 0.0f;
        float total = 0.0f;
        for (float ratio : ratios) {
            total += ratio;
        }
        if (total > 0.0f && band.index <= ratios.size()) {
            for (uint32_t k = 0; k < band.index; ++k) {
                before += ratios[k] / total;
            }
        } else if (count > 0) {
            before = static_cast<float>(band.index) / static_cast<float>(count);
        }
        return fraction - before;
    }

    DockPosition ResolveDockPosition(const LayoutRect &rect, float x, float y, float edgeFraction)
    {
        const float w = rect.Width();
        const float h = rect.Height();
        if (w <= 0.0f || h <= 0.0f) {
            return DockPosition::CENTER;
        }
        const float fx = (x - rect.left) / w;
        const float fy = (y - rect.top) / h;
        const float edge = std::clamp(edgeFraction, 0.0f, 0.5f);

        const float dl = fx;
        const float dr = 1.0f - fx;
        const float dt = fy;
        const float db = 1.0f - fy;
        const float nearest = std::min(std::min(dl, dr), std::min(dt, db));
        if (nearest >= edge) {
            return DockPosition::CENTER;
        }
        if (nearest == dl) {
            return DockPosition::LEFT;
        }
        if (nearest == dr) {
            return DockPosition::RIGHT;
        }
        if (nearest == dt) {
            return DockPosition::TOP;
        }
        return DockPosition::BOTTOM;
    }

} // namespace sky::editor
