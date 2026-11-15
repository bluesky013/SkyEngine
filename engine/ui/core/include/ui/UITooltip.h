//
// Created on 2026/11/15.
//

#pragma once

#include <ui/UIRect.h>

#include <cstdint>
#include <memory>

namespace sky::ui {

    struct UITooltipTarget {
        uint64_t id = 0;
        UIRect   anchor;
        // Identity is stable within this token; rebinding must invalidate the token or change id.
        std::weak_ptr<void> lifetime;
    };

} // namespace sky::ui
