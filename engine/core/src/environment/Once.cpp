//
// Created on 2026/10/10.
//

#include <core/environment/Once.h>

namespace sky {

    bool Once::Run(std::string_view key)
    {
        return keys.emplace(key).second;
    }

} // namespace sky
