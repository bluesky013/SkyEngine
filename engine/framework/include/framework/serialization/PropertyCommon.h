//
// Created by Zach Lee on 2021/12/16.
//

#pragma once

#include <cstdint>

namespace sky {

    enum class CommonPropertyKey : uint32_t {
        VISIBLE,
        LABEL_VISIBLE,
        LABEL_COLOR,
        ASSET_TYPE,
        REPLICATED,
        // UI attributes (appended so existing values stay stable).
        LABEL,
        TOOLTIP,
        ORDER,
        CATEGORY,
        READONLY,
        MULTILINE,
        RANGE_MIN,
        RANGE_MAX,
        RANGE_STEP,
        EDITOR_HINT,
        EDITOR_KIND,
        ENUM_FLAGS,
        COLOR_SPACE
    };

}
