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


// 检测操作系统平台
#if defined(_WIN32) || defined(_WIN64)
    #define SKY_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)

#include "TargetConditionals.h"
    #if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
        #define SKY_PLATFORM_IOS 1
    #elif TARGET_OS_MAC
        #define SKY_PLATFORM_MACOS 1
    #endif

#elif defined(__ANDROID__)
    #define SKY_PLATFORM_ANDROID 1
#elif defined(__linux__)
    #define SKY_PLATFORM_LINUX 1
#else
    #define SKY_PLATFORM_UNKNOWN 1
#endif

#if defined(__clang__)
    #define SKY_PLATFORM_COMPILER_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
    #define SKY_PLATFORM_COMPILER_GCC 1
#elif defined(_MSC_VER)
    #define SKY_PLATFORM_COMPILER_MSVC 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #define SKY_PLATFORM_ARCH_X64 1
#elif defined(__i386__) || defined(_M_IX86)
    #define SKY_PLATFORM_ARCH_X86 1
#elif defined(__arm__) || defined(_M_ARM)
    #define SKY_PLATFORM_ARCH_ARM 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define SKY_PLATFORM_ARCH_ARM64 1
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
