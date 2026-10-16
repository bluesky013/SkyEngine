#pragma once

#include <functional>

namespace sky::editor::detail {
    // An injected guard is the boundary authorizing borrowed-object access.
    // If the provider cannot complete it, stop with Conflict instead of using
    // state that may have been invalidated by that callback.
    inline bool CheckPreferenceGuard(const std::function<bool()> &guard)
    {
        if (!guard) {
            return true;
        }
        try {
            return guard();
        }
        catch (...) {
            return false;
        }
    }
} // namespace sky::editor::detail
