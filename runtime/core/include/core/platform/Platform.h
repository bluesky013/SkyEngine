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

#ifndef SKY_CPU_LITTLE_ENDIAN
    #if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        #define SKY_CPU_LITTLE_ENDIAN 1
    #elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        #define SKY_CPU_LITTLE_ENDIAN 0
    #else
        #define SKY_CPU_LITTLE_ENDIAN 1
    #endif
#endif

#include <cstdint>

namespace sky {

    enum class PlatformType : uint8_t {
        Default,
        Windows,
        MacOS,
        Linux,
        Android,
        IOS,
        UNDEFINED
    };

    enum class Endian : uint8_t {
        Little,
        Big
    };

    inline uint16_t SwapEndian16(uint16_t value)
    {
        return (value >> 8) | (value << 8);
    }

    inline uint32_t SwapEndian32(uint32_t value)
    {
        return ((value >> 24) & 0x000000FF) |
               ((value >> 8)  & 0x0000FF00) |
               ((value << 8)  & 0x00FF0000) |
               ((value << 24) & 0xFF000000);
    }

    inline uint64_t SwapEndian64(uint64_t value)
    {
        return ((value >> 56) & 0x00000000000000FFULL) |
               ((value >> 40) & 0x000000000000FF00ULL) |
               ((value >> 24) & 0x0000000000FF0000ULL) |
               ((value >> 8)  & 0x00000000FF000000ULL) |
               ((value << 8)  & 0x000000FF00000000ULL) |
               ((value << 24) & 0x0000FF0000000000ULL) |
               ((value << 40) & 0x00FF000000000000ULL) |
               ((value << 56) & 0xFF00000000000000ULL);
    }

    inline bool IsLittleEndian()
    {
        const union { uint32_t u; uint8_t c[4]; } one = { 1 };
        return one.c[0] != 0u;
    }

} // namespace sky
