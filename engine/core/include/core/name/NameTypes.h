//
// Created by blues on 2024/11/22.
//

#pragma once

#include <string_view>
#include <cstdint>

namespace sky {

    struct NameStorageHeader {
        uint16_t current = 0;
    };

    struct NameStorageHandle {
        uint16_t block;
        uint16_t offset;
    };

    using NameEntryHandle = uint32_t;

    struct NameEntry {
        NameStorageHandle handle;
        uint16_t length;
    };

} // namespace sky
