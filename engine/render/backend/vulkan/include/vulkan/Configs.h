//
// Created by blues on 2024/9/17.
//

#pragma once

#if defined(_DEBUG) && defined(_WIN32)
    #define VMA_STATS_STRING_ENABLED 1
#else
    #define VMA_STATS_STRING_ENABLED 0
#endif
