//
// Created by SkyEngine on 2024/02/15.
//

#pragma once

#include <cstdint>
#include <cstring>

// Platform detection and SIMD include
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define SKY_SIMD_SSE 1
    #include <emmintrin.h>  // SSE2
    #include <xmmintrin.h>  // SSE
    #if defined(__SSE4_1__) || defined(__AVX__)
        #include <smmintrin.h>  // SSE4.1
        #define SKY_SIMD_SSE4 1
    #endif
    #if defined(__AVX2__)
        #include <immintrin.h>  // AVX2
        #define SKY_SIMD_AVX2 1
    #endif
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define SKY_SIMD_NEON 1
    #include <arm_neon.h>
#else
    #define SKY_SIMD_NONE 1
#endif

#ifdef _MSC_VER
    #include <intrin.h>
#endif

namespace sky {
namespace simd {

    /**
     * @brief SIMD-accelerated bitwise OR of two uint64_t arrays
     * @param dst Destination array (also first source)
     * @param src Source array to OR with
     * @param count Number of uint64_t elements
     */
    inline void BitwiseOr(uint64_t* dst, const uint64_t* src, size_t count)
    {
#if defined(SKY_SIMD_AVX2)
        // Process 4 uint64_t (256 bits) at a time with AVX2
        size_t simdCount = count / 4;
        for (size_t i = 0; i < simdCount; ++i) {
            __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i * 4));
            __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 4));
            __m256i result = _mm256_or_si256(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 4), result);
        }
        // Handle remaining elements
        for (size_t i = simdCount * 4; i < count; ++i) {
            dst[i] |= src[i];
        }
#elif defined(SKY_SIMD_SSE)
        // Process 2 uint64_t (128 bits) at a time with SSE2
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + i * 2));
            __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 2));
            __m128i result = _mm_or_si128(a, b);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 2), result);
        }
        // Handle remaining element
        for (size_t i = simdCount * 2; i < count; ++i) {
            dst[i] |= src[i];
        }
#elif defined(SKY_SIMD_NEON)
        // Process 2 uint64_t (128 bits) at a time with NEON
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            uint64x2_t a = vld1q_u64(dst + i * 2);
            uint64x2_t b = vld1q_u64(src + i * 2);
            uint64x2_t result = vorrq_u64(a, b);
            vst1q_u64(dst + i * 2, result);
        }
        // Handle remaining element
        for (size_t i = simdCount * 2; i < count; ++i) {
            dst[i] |= src[i];
        }
#else
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            dst[i] |= src[i];
        }
#endif
    }

    /**
     * @brief SIMD-accelerated bitwise AND of two uint64_t arrays
     * @param dst Destination array (also first source)
     * @param src Source array to AND with
     * @param count Number of uint64_t elements
     */
    inline void BitwiseAnd(uint64_t* dst, const uint64_t* src, size_t count)
    {
#if defined(SKY_SIMD_AVX2)
        size_t simdCount = count / 4;
        for (size_t i = 0; i < simdCount; ++i) {
            __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i * 4));
            __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 4));
            __m256i result = _mm256_and_si256(a, b);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 4), result);
        }
        for (size_t i = simdCount * 4; i < count; ++i) {
            dst[i] &= src[i];
        }
#elif defined(SKY_SIMD_SSE)
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + i * 2));
            __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 2));
            __m128i result = _mm_and_si128(a, b);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 2), result);
        }
        for (size_t i = simdCount * 2; i < count; ++i) {
            dst[i] &= src[i];
        }
#elif defined(SKY_SIMD_NEON)
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            uint64x2_t a = vld1q_u64(dst + i * 2);
            uint64x2_t b = vld1q_u64(src + i * 2);
            uint64x2_t result = vandq_u64(a, b);
            vst1q_u64(dst + i * 2, result);
        }
        for (size_t i = simdCount * 2; i < count; ++i) {
            dst[i] &= src[i];
        }
#else
        for (size_t i = 0; i < count; ++i) {
            dst[i] &= src[i];
        }
#endif
    }

    /**
     * @brief SIMD-accelerated check if any bit is set in uint64_t array
     * @param data Array to check
     * @param count Number of uint64_t elements
     * @return true if any bit is set
     */
    inline bool AnyBitSet(const uint64_t* data, size_t count)
    {
#if defined(SKY_SIMD_AVX2)
        size_t simdCount = count / 4;
        for (size_t i = 0; i < simdCount; ++i) {
            __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i * 4));
            if (!_mm256_testz_si256(v, v)) {
                return true;
            }
        }
        for (size_t i = simdCount * 4; i < count; ++i) {
            if (data[i] != 0) {
                return true;
            }
        }
        return false;
#elif defined(SKY_SIMD_SSE4)
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i * 2));
            if (!_mm_testz_si128(v, v)) {
                return true;
            }
        }
        for (size_t i = simdCount * 2; i < count; ++i) {
            if (data[i] != 0) {
                return true;
            }
        }
        return false;
#elif defined(SKY_SIMD_SSE)
        // SSE2 fallback using comparison
        __m128i zero = _mm_setzero_si128();
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i * 2));
            __m128i cmp = _mm_cmpeq_epi32(v, zero);
            if (_mm_movemask_epi8(cmp) != 0xFFFF) {
                return true;
            }
        }
        for (size_t i = simdCount * 2; i < count; ++i) {
            if (data[i] != 0) {
                return true;
            }
        }
        return false;
#elif defined(SKY_SIMD_NEON)
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            uint64x2_t v = vld1q_u64(data + i * 2);
            uint64x2_t zero = vdupq_n_u64(0);
            uint64x2_t cmp = vceqq_u64(v, zero);
            // Check if any lane is not zero
            if (vgetq_lane_u64(cmp, 0) != ~0ULL || vgetq_lane_u64(cmp, 1) != ~0ULL) {
                return true;
            }
        }
        for (size_t i = simdCount * 2; i < count; ++i) {
            if (data[i] != 0) {
                return true;
            }
        }
        return false;
#else
        for (size_t i = 0; i < count; ++i) {
            if (data[i] != 0) {
                return true;
            }
        }
        return false;
#endif
    }

    /**
     * @brief Population count for a single uint64_t
     * @param value Value to count bits in
     * @return Number of set bits
     */
    inline uint32_t PopCount64(uint64_t value)
    {
#ifdef _MSC_VER
        return static_cast<uint32_t>(__popcnt64(value));
#else
        return static_cast<uint32_t>(__builtin_popcountll(value));
#endif
    }

    /**
     * @brief SIMD-accelerated population count for uint64_t array
     * @param data Array to count bits in
     * @param count Number of uint64_t elements
     * @return Total number of set bits
     */
    inline uint32_t PopCountArray(const uint64_t* data, size_t count)
    {
        uint32_t total = 0;
        
#if defined(SKY_SIMD_AVX2) && defined(__AVX2__)
        // AVX2 popcount using parallel technique
        // Note: Hardware popcount via POPCNT instruction is typically used per-word
        // This still processes sequentially but benefits from cache prefetching
        for (size_t i = 0; i < count; ++i) {
            total += PopCount64(data[i]);
        }
#else
        // Use scalar popcount (which often uses hardware POPCNT instruction)
        for (size_t i = 0; i < count; ++i) {
            total += PopCount64(data[i]);
        }
#endif
        return total;
    }

    /**
     * @brief SIMD-accelerated zero fill for uint64_t array
     * @param dst Destination array
     * @param count Number of uint64_t elements
     */
    inline void ZeroFill(uint64_t* dst, size_t count)
    {
#if defined(SKY_SIMD_AVX2)
        __m256i zero = _mm256_setzero_si256();
        size_t simdCount = count / 4;
        for (size_t i = 0; i < simdCount; ++i) {
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 4), zero);
        }
        for (size_t i = simdCount * 4; i < count; ++i) {
            dst[i] = 0;
        }
#elif defined(SKY_SIMD_SSE)
        __m128i zero = _mm_setzero_si128();
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 2), zero);
        }
        for (size_t i = simdCount * 2; i < count; ++i) {
            dst[i] = 0;
        }
#elif defined(SKY_SIMD_NEON)
        uint64x2_t zero = vdupq_n_u64(0);
        size_t simdCount = count / 2;
        for (size_t i = 0; i < simdCount; ++i) {
            vst1q_u64(dst + i * 2, zero);
        }
        for (size_t i = simdCount * 2; i < count; ++i) {
            dst[i] = 0;
        }
#else
        std::memset(dst, 0, count * sizeof(uint64_t));
#endif
    }

    /**
     * @brief SIMD-accelerated AABB-AABB intersection test
     * 
     * Tests intersection between multiple AABB pairs efficiently
     * 
     * @param minA Min corners of AABBs A (xyz triplets, count * 3 floats)
     * @param maxA Max corners of AABBs A
     * @param minB Min corners of AABBs B
     * @param maxB Max corners of AABBs B
     * @param results Output: 1 if intersecting, 0 if not
     * @param count Number of AABB pairs to test
     */
    inline void AABBIntersectionBatch(
        const float* minA, const float* maxA,
        const float* minB, const float* maxB,
        uint8_t* results, size_t count)
    {
#if defined(SKY_SIMD_SSE)
        // Process one AABB pair at a time using SSE
        for (size_t i = 0; i < count; ++i) {
            size_t offset = i * 3;
            
            // Load AABB corners (xyz)
            // minA[x,y,z], maxA[x,y,z], minB[x,y,z], maxB[x,y,z]
            __m128 aMin = _mm_set_ps(0.0f, minA[offset + 2], minA[offset + 1], minA[offset + 0]);
            __m128 aMax = _mm_set_ps(0.0f, maxA[offset + 2], maxA[offset + 1], maxA[offset + 0]);
            __m128 bMin = _mm_set_ps(0.0f, minB[offset + 2], minB[offset + 1], minB[offset + 0]);
            __m128 bMax = _mm_set_ps(0.0f, maxB[offset + 2], maxB[offset + 1], maxB[offset + 0]);
            
            // Test: aMin <= bMax && bMin <= aMax for all axes
            __m128 test1 = _mm_cmple_ps(aMin, bMax);
            __m128 test2 = _mm_cmple_ps(bMin, aMax);
            __m128 combined = _mm_and_ps(test1, test2);
            
            // Extract results (check x, y, z lanes)
            int mask = _mm_movemask_ps(combined);
            // Bits 0,1,2 correspond to x,y,z. All must be set (0x7)
            results[i] = (mask & 0x7) == 0x7 ? 1 : 0;
        }
#elif defined(SKY_SIMD_NEON)
        for (size_t i = 0; i < count; ++i) {
            size_t offset = i * 3;
            
            // Load and test each axis
            float32x4_t aMin = {minA[offset + 0], minA[offset + 1], minA[offset + 2], 0.0f};
            float32x4_t aMax = {maxA[offset + 0], maxA[offset + 1], maxA[offset + 2], 0.0f};
            float32x4_t bMin = {minB[offset + 0], minB[offset + 1], minB[offset + 2], 0.0f};
            float32x4_t bMax = {maxB[offset + 0], maxB[offset + 1], maxB[offset + 2], 0.0f};
            
            uint32x4_t test1 = vcleq_f32(aMin, bMax);
            uint32x4_t test2 = vcleq_f32(bMin, aMax);
            uint32x4_t combined = vandq_u32(test1, test2);
            
            // Check if x, y, z all pass
            results[i] = (vgetq_lane_u32(combined, 0) && 
                          vgetq_lane_u32(combined, 1) && 
                          vgetq_lane_u32(combined, 2)) ? 1 : 0;
        }
#else
        // Scalar fallback
        for (size_t i = 0; i < count; ++i) {
            size_t offset = i * 3;
            bool intersects = 
                (minA[offset + 0] <= maxB[offset + 0] && maxA[offset + 0] >= minB[offset + 0]) &&
                (minA[offset + 1] <= maxB[offset + 1] && maxA[offset + 1] >= minB[offset + 1]) &&
                (minA[offset + 2] <= maxB[offset + 2] && maxA[offset + 2] >= minB[offset + 2]);
            results[i] = intersects ? 1 : 0;
        }
#endif
    }

    /**
     * @brief SIMD-accelerated squared distance calculation
     * @param pointsA First set of points (xyz triplets)
     * @param pointsB Second set of points (xyz triplets)
     * @param distancesSq Output squared distances
     * @param count Number of point pairs
     */
    inline void DistanceSquaredBatch(
        const float* pointsA,
        const float* pointsB,
        float* distancesSq,
        size_t count)
    {
#if defined(SKY_SIMD_SSE)
        for (size_t i = 0; i < count; ++i) {
            size_t offset = i * 3;
            
            __m128 a = _mm_set_ps(0.0f, pointsA[offset + 2], pointsA[offset + 1], pointsA[offset + 0]);
            __m128 b = _mm_set_ps(0.0f, pointsB[offset + 2], pointsB[offset + 1], pointsB[offset + 0]);
            
            __m128 diff = _mm_sub_ps(a, b);
            __m128 sq = _mm_mul_ps(diff, diff);
            
            // Sum x^2 + y^2 + z^2 using SSE2-compatible horizontal sum
            // sq = [x^2, y^2, z^2, 0]
            __m128 shuf = _mm_shuffle_ps(sq, sq, _MM_SHUFFLE(2, 3, 0, 1)); // [y^2, x^2, 0, z^2]
            __m128 sums = _mm_add_ps(sq, shuf);  // [x^2+y^2, y^2+x^2, z^2+0, 0+z^2]
            shuf = _mm_movehl_ps(shuf, sums);    // [z^2+0, 0+z^2, ?, ?]
            sums = _mm_add_ss(sums, shuf);       // [x^2+y^2+z^2, ...]
            
            _mm_store_ss(&distancesSq[i], sums);
        }
#elif defined(SKY_SIMD_NEON)
        for (size_t i = 0; i < count; ++i) {
            size_t offset = i * 3;
            
            float32x4_t a = {pointsA[offset + 0], pointsA[offset + 1], pointsA[offset + 2], 0.0f};
            float32x4_t b = {pointsB[offset + 0], pointsB[offset + 1], pointsB[offset + 2], 0.0f};
            
            float32x4_t diff = vsubq_f32(a, b);
            float32x4_t sq = vmulq_f32(diff, diff);
            
            // Sum horizontally
            float32x2_t sum = vadd_f32(vget_low_f32(sq), vget_high_f32(sq));
            sum = vpadd_f32(sum, sum);
            
            distancesSq[i] = vget_lane_f32(sum, 0);
        }
#else
        for (size_t i = 0; i < count; ++i) {
            size_t offset = i * 3;
            float dx = pointsA[offset + 0] - pointsB[offset + 0];
            float dy = pointsA[offset + 1] - pointsB[offset + 1];
            float dz = pointsA[offset + 2] - pointsB[offset + 2];
            distancesSq[i] = dx * dx + dy * dy + dz * dz;
        }
#endif
    }

} // namespace simd
} // namespace sky
