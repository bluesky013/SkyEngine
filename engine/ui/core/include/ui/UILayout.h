//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIRect.h>

#include <cstdint>

namespace sky::ui {

    enum class UISizeMode : uint8_t {
        AUTO = 0,
        FILL,
        FIXED,
        PERCENT,
    };

    struct UISizeRule {
        UISizeMode mode = UISizeMode::AUTO;
        float value = 0.0f;
    };

    struct UILayoutParams {
        float anchorMinX = 0.0f;
        float anchorMinY = 0.0f;
        float anchorMaxX = 0.0f;
        float anchorMaxY = 0.0f;

        float offsetLeft = 0.0f;
        float offsetTop = 0.0f;
        float offsetRight = 0.0f;
        float offsetBottom = 0.0f;

        UISizeRule width;
        UISizeRule height;

        // Paint/hit-test order among siblings; higher z is drawn later.
        float z = 0.0f;

        float paddingLeft = 0.0f;
        float paddingTop = 0.0f;
        float paddingRight = 0.0f;
        float paddingBottom = 0.0f;

        // Inner rect used to arrange children (bounds shrunk by padding).
        UIRect ContentRect(const UIRect &bounds) const;
    };

    // Resolves an element's bounds from its layout rule against the parent rect.
    // An axis with anchorMin < anchorMax stretches between the anchors and uses
    // the offsets as insets; an axis with anchorMin == anchorMax is a point
    // anchor and uses the size rule (FIXED value / FILL / measured AUTO size).
    UIRect ComputeElementBounds(const UILayoutParams &layout,
                                const UIRect &parentRect,
                                float measuredWidth,
                                float measuredHeight);

} // namespace sky::ui
