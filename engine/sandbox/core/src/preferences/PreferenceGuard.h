#pragma once

#include <functional>

namespace sky::editor::detail {
    // A failed guard prevents access to invalidated borrowed settings.
    inline bool CheckPreferenceGuard(const std::function<bool()> &guard)
    {
        return !guard || guard();
    }
} // namespace sky::editor::detail
