//
// Created for SIMD math support.
//

#pragma once

// SIMD platform detection
#if defined(SKY_MATH_SIMD) && SKY_MATH_SIMD

    #if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
        #define SKY_SIMD_SSE 1
        #include <xmmintrin.h> // SSE
        #include <emmintrin.h> // SSE2
    #endif

    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define SKY_SIMD_NEON 1
        #include <arm_neon.h>
    #endif

#endif

#ifndef SKY_SIMD_SSE
    #define SKY_SIMD_SSE 0
#endif

#ifndef SKY_SIMD_NEON
    #define SKY_SIMD_NEON 0
#endif

#define SKY_SIMD_ENABLED (SKY_SIMD_SSE || SKY_SIMD_NEON)
