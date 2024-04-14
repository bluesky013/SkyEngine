//
// Created by blues on 2024/3/7.
//

#include <rhi/Util.h>

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

} // namespace sky::rhi