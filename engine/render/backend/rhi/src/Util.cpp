//
// Created by blues on 2024/3/7.
//

#include <rhi/Util.h>
#include <cstdint>

namespace sky::rhi {

    std::vector<const char*> ParseExtensionString(char* names)
    {
        std::vector<const char*> list;
        while (names != nullptr && *names != 0) {
            list.push_back(names);
            while (*(++names) != 0) {
                if (*names == ' ') {
                    *names++ = '\0';
                    break;
                }
            }
        }
        return list;
    }

    uint32_t GetMipLevel(uint32_t width, uint32_t height)
    {
        uint32_t size = std::max(width, height);
        uint32_t level = 0;
        while (size != 0) {
            size >>= 1;
            ++level;
        }
        return level;
    }

} // namespace sky::rhi