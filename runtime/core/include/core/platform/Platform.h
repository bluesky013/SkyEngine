//
// Created by Zach Lee on 2021/11/11.
//

#pragma once

#ifdef _DEBUG
#include <cassert>
#define SKY_ASSERT(val) assert(val);
#else
#define SKY_ASSERT(val) (val);
#endif

#ifdef _DEBUG
#define SKY_UNEXPECTED assert(false && "unexpected code")
#else
#define SKY_UNEXPECTED
#endif

#ifdef _MSC_VER
    #define SKY_EXPORT __declspec(dllexport)
    #define SKY_IMPORT __declspec(dllimport)
#else
    #define SKY_EXPORT __attribute__((visibility("default")))
    #define SKY_IMPORT __attribute__((visibility("default")))
#endif

#include <cstdint>

namespace sky {

    enum class PlatformType : uint32_t {
        Default,
        Windows,
        MacOS,
        Linux,
        Android,
        IOS,
        UNDEFINED
    };

} // namespace sky