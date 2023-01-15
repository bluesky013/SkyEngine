//
// Created by Zach Lee on 2022/8/12.
//

#pragma once

#include <string>
#include <core/util/ArrayBitFlag.h>

namespace sky::editor {

    enum class DocumentFlagBit : uint32_t {
        PROJECT_OPEN = 1,
        LEVEL_OPEN   = 2,
        MAX
    };

    using DocFlagArray = ArrayBit<DocumentFlagBit, static_cast<uint32_t>(DocumentFlagBit::MAX)>;
} // namespace sky::editor