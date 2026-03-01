//
// Created by blues on 2025/12/30.
//

#pragma once

#include <core/platform/Platform.h>

namespace sky {

    template <typename T>
    FORCEINLINE size_t ToSize(const T& val)
    {
        return static_cast<size_t>(val);
    }

    template <typename T, typename U>
    FORCEINLINE T Cast(const U& val)
    {
        return static_cast<T>(val);
    }

} // namespace sky
