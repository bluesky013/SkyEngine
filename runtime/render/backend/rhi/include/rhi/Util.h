//
// Created by blues on 2024/3/7.
//

#pragma once

#include <vector>

namespace sky::rhi {

    std::vector<const char*> ParseExtensionString(char* names);
    uint32_t GetMipLevel(uint32_t width, uint32_t height);
} // namespace sky::rhi