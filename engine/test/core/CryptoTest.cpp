//
// Created by blues on 2024/12/29.
//

#include <core/crypto/md5/MD5.h>
#include <core/crypto/crc32/Crc32.h>
#include <core/hash/Crc32.h>
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cstring>
#include <numeric>

using namespace sky;

// Helper: build deterministic pattern data (byte = i % 251, prime avoids alignment artifacts)
static std::vector<uint8_t> MakePattern(size_t size)
{
    std::vector<uint8_t> v(size);
    for (size_t i = 0; i < size; ++i) v[i] = static_cast<uint8_t>(i % 251);
    return v;
}

// ===========================================================================
// MD5 Tests
// ===========================================================================

// RFC 1321 - Appendix A.5 test suite
TEST(CryptoTest, MD5Test)
{
    ASSERT_EQ(MD5::CalculateMD5("").ToString(), "d41d8cd98f00b204e9800998ecf8427e");
    ASSERT_EQ(MD5::CalculateMD5("a").ToString(), "0cc175b9c0f1b6a831c399e269772661");
    ASSERT_EQ(MD5::CalculateMD5("abc").ToString(), "900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(MD5::CalculateMD5("message digest").ToString(), "f96b697d7cb7938d525a2f31aaf161d0");
    ASSERT_EQ(MD5::CalculateMD5("abcdefghijklmnopqrstuvwxyz").ToString(), "c3fcd3d76192e4007dfb496cca67e13b");
    ASSERT_EQ(MD5::CalculateMD5("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789").ToString(), "d174ab98d277d9f5a5611c2c9f419d9f");
    ASSERT_EQ(MD5::CalculateMD5("12345678901234567890123456789012345678901234567890123456789012345678901234567890").ToString(), "57edf4a22be3c955ac49da2e2107b67a");
}

// ===========================================================================
// CRC32C Tests — basic
// ===========================================================================

TEST(CryptoTest, CRC32CEmpty)
{
    ASSERT_EQ(Crc32C(nullptr, 0), 0x00000000u);
}

TEST(CryptoTest, CRC32CSingleByte)
{
    uint8_t z = 0x00;
    ASSERT_EQ(Crc32C(&z, 1), 0x527D5351u);

    uint8_t f = 0xFF;
    ASSERT_EQ(Crc32C(&f, 1), 0xFF000000u);

    uint8_t a = 'A';
    ASSERT_EQ(Crc32C(&a, 1), 0xE16DCDEEu);
}

TEST(CryptoTest, CRC32CKnownString)
{
    // CRC32C of "123456789" = 0xE3069283 (standard check value)
    const std::string digits = "123456789";
    ASSERT_EQ(Crc32C(reinterpret_cast<const uint8_t *>(digits.data()), digits.size()), 0xE3069283u);
}

TEST(CryptoTest, CRC32CHashWrapperConsistency)
{
    const std::string digits = "123456789";
    ASSERT_EQ(Crc32::Cal(digits), Crc32C(reinterpret_cast<const uint8_t *>(digits.data()), digits.size()));
}

TEST(CryptoTest, CRC32CAllZeros)
{
    // Verify CRC32C of all-zero buffers at various sizes covering tail paths
    struct { size_t size; uint32_t expected; } cases[] = {
        {  1, 0x527D5351u}, {  7, 0xBB3E6A6Du}, {  8, 0x8C28B28Au},
        { 15, 0x530ED410u}, { 16, 0x42709AEAu}, { 63, 0x9062E550u},
        { 64, 0x03C8EB67u}, {100, 0x07CB9FF6u}, {256, 0xB872B190u},
    };
    for (const auto &c : cases) {
        std::vector<uint8_t> buf(c.size, 0);
        ASSERT_EQ(Crc32C(buf.data(), buf.size()), c.expected)
            << "Failed for zeros(" << c.size << ")";
    }
}

TEST(CryptoTest, CRC32CAllOnes)
{
    std::vector<uint8_t> buf(32, 0xFF);
    ASSERT_EQ(Crc32C(buf.data(), buf.size()), 0x62A8AB43u);
}

// ===========================================================================
// CRC32C Tests — incremental (2-arg overload)
// ===========================================================================

TEST(CryptoTest, CRC32CIncrementalSmall)
{
    const std::string part1 = "12345";
    const std::string part2 = "6789";

    uint32_t crc = Crc32C(reinterpret_cast<const uint8_t *>(part1.data()), part1.size());
    crc = Crc32C(reinterpret_cast<const uint8_t *>(part2.data()), part2.size(), crc);

    const std::string full = "123456789";
    ASSERT_EQ(crc, Crc32C(reinterpret_cast<const uint8_t *>(full.data()), full.size()));
}

TEST(CryptoTest, CRC32CIncrementalLarge)
{
    // 20000 bytes split at 12288 (Tier 0 boundary)
    auto data = MakePattern(20000);
    uint32_t full = Crc32C(data.data(), data.size());

    uint32_t crc = Crc32C(data.data(), 12288);
    crc = Crc32C(data.data() + 12288, data.size() - 12288, crc);
    ASSERT_EQ(crc, full);
}

TEST(CryptoTest, CRC32CIncrementalMultiPart)
{
    // 50000 bytes split into 3 unequal parts spanning all tiers
    auto data = MakePattern(50000);
    uint32_t full = Crc32C(data.data(), data.size());

    uint32_t crc = Crc32C(data.data(), 15000);
    crc = Crc32C(data.data() + 15000, 20000, crc);
    crc = Crc32C(data.data() + 35000, 15000, crc);
    ASSERT_EQ(crc, full);
}

// ===========================================================================
// CRC32C Tests — tier boundary coverage
//
// Hardware path tiers:
//   Tier 0: size >= 3×4096 = 12288   (8× unrolled, 3-way parallel)
//   Tier 1: size >= 3×1360 = 4080    (3-way parallel)
//   Tier 2: size >= 3×336  = 1008    (3-way parallel)
//   Tail:   size < 1008              (single-stream)
// ===========================================================================

TEST(CryptoTest, CRC32CTierBoundaries)
{
    // Test at exact thresholds and ±1 to cover edge cases
    struct { size_t size; uint32_t expected; } cases[] = {
        { 1007, 0xF71A9321u},  // just below Tier 2
        { 1008, 0x930CB7EBu},  // exact Tier 2 threshold
        { 1009, 0xEFCDCBD8u},  // just above Tier 2
        { 4079, 0xF0A85A19u},  // just below Tier 1
        { 4080, 0x5491923Du},  // exact Tier 1 threshold
        { 4081, 0x9C99DA1Du},  // just above Tier 1
        {12287, 0x4DC24BEDu},  // just below Tier 0
        {12288, 0xB30BE1EDu},  // exact Tier 0 threshold
        {12289, 0xFDB8D30Cu},  // just above Tier 0
    };

    for (const auto &c : cases) {
        auto data = MakePattern(c.size);
        ASSERT_EQ(Crc32C(data.data(), data.size()), c.expected)
            << "Failed for pattern(" << c.size << ")";
    }
}

TEST(CryptoTest, CRC32CLargeBlocks)
{
    // 16 KB — exercises Tier 0 once + Tier 1 once + tail
    auto d16k = MakePattern(16384);
    ASSERT_EQ(Crc32C(d16k.data(), d16k.size()), 0xEAFCA51Du);

    // 64 KB — exercises Tier 0 multiple times
    auto d64k = MakePattern(65536);
    ASSERT_EQ(Crc32C(d64k.data(), d64k.size()), 0x0DAAFCDEu);
}

TEST(CryptoTest, CRC32C1MB)
{
    // 1 MB of 0xAB — a known constant for benchmark cross-check
    std::vector<uint8_t> buf(1024 * 1024, 0xAB);
    ASSERT_EQ(Crc32C(buf.data(), buf.size()), 0xF8F79F82u);
}

// ===========================================================================
// CRC32 IEEE Tests
// ===========================================================================

TEST(CryptoTest, CRC32IEEEEmpty)
{
    ASSERT_EQ(CalcCrc32(nullptr, 0), 0x00000000u);
}

TEST(CryptoTest, CRC32IEEEKnownString)
{
    const std::string digits = "123456789";
    ASSERT_EQ(CalcCrc32(reinterpret_cast<const uint8_t *>(digits.data()), digits.size()), 0xCBF43926u);
}

TEST(CryptoTest, CRC32IEEEIncremental)
{
    const std::string part1 = "12345";
    const std::string part2 = "6789";

    uint32_t crc = CalcCrc32(reinterpret_cast<const uint8_t *>(part1.data()), part1.size());
    crc = CalcCrc32(reinterpret_cast<const uint8_t *>(part2.data()), part2.size(), crc);

    const std::string full = "123456789";
    ASSERT_EQ(crc, CalcCrc32(reinterpret_cast<const uint8_t *>(full.data()), full.size()));
}

TEST(CryptoTest, CRC32IEEELargeData)
{
    // Verify at tier boundaries (software-only, so tiers don't apply, but data size still matters)
    struct { size_t size; uint32_t expected; } cases[] = {
        { 1008, 0x8BC1C062u},
        { 4080, 0x659AC28Cu},
        {12288, 0x7553287Du},
    };
    for (const auto &c : cases) {
        auto data = MakePattern(c.size);
        ASSERT_EQ(CalcCrc32(data.data(), data.size()), c.expected)
            << "Failed for CRC32 IEEE pattern(" << c.size << ")";
    }
}

// ===========================================================================
// Performance benchmarks (printed, not asserted on speed)
// ===========================================================================

TEST(CryptoTest, CRC32CPerformance)
{
    constexpr size_t SIZE = 1024 * 1024;
    constexpr int ITERATIONS = 100;
    std::vector<uint8_t> buf(SIZE, 0xAB);

    volatile uint32_t sink = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        sink = Crc32C(buf.data(), buf.size());
    }
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double totalMB = static_cast<double>(SIZE) * ITERATIONS / (1024.0 * 1024.0);
    double throughput = totalMB / (ms / 1000.0);

    printf("  CRC32C: %.1f MB in %.1f ms  =>  %.0f MB/s\n", totalMB, ms, throughput);

    ASSERT_EQ(sink, 0xF8F79F82u);
}

TEST(CryptoTest, MD5Performance)
{
    constexpr size_t SIZE = 1024 * 1024;
    constexpr int ITERATIONS = 100;
    std::vector<char> buf(SIZE, 0xAB);

    volatile uint64_t sink = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        auto md5 = MD5::CalculateMD5(buf.data(), buf.size());
        sink = md5.u64[0];
    }
    auto end = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double totalMB = static_cast<double>(SIZE) * ITERATIONS / (1024.0 * 1024.0);
    double throughput = totalMB / (ms / 1000.0);

    printf("  MD5:    %.1f MB in %.1f ms  =>  %.0f MB/s\n", totalMB, ms, throughput);

    ASSERT_NE(sink, 0u);
}