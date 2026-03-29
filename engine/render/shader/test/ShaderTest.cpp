//
// Created by blues on 2024/12/30.
//

#include <gtest/gtest.h>
#include <shader/ShaderVariant.h>

using namespace sky;

TEST(ShaderTest, ShaderVariantKeyTest)
{
    ShaderVariantKey key = {};

    key.SetValue(0, 1, 2);
    key.SetValue(28, 35, 127);

    ASSERT_EQ(key.GetValue(28, 35), 127);
    ASSERT_EQ(key.GetValue(0, 1), 2);
}

// --- SetValue / GetValue: single-bit at every byte boundary ---
TEST(ShaderTest, SetGetSingleBit)
{
    // Test single-bit set/get at bit 0 of each byte
    for (uint8_t bit = 0; bit < 64; bit += 8) {
        ShaderVariantKey key = {};
        key.SetValue(bit, bit, 1);
        ASSERT_EQ(key.GetValue(bit, bit), 1) << "bit=" << (int)bit;

        // Other bits in the same byte should be 0
        if (bit + 1 < 64) {
            ASSERT_EQ(key.GetValue(bit + 1, bit + 1), 0) << "bit+1=" << (int)(bit + 1);
        }
    }

    // Test single-bit set/get at bit 7 of each byte (last bit in byte)
    for (uint8_t bit = 7; bit < 64; bit += 8) {
        ShaderVariantKey key = {};
        key.SetValue(bit, bit, 1);
        ASSERT_EQ(key.GetValue(bit, bit), 1) << "bit=" << (int)bit;
    }
}

// --- SetValue / GetValue: within a single byte (no cross-boundary) ---
TEST(ShaderTest, SetGetWithinSingleByte)
{
    {
        // Bits [0,3] -- 4-bit value within byte 0
        ShaderVariantKey key = {};
        key.SetValue(0, 3, 0x0F);
        ASSERT_EQ(key.GetValue(0, 3), 0x0F);
    }
    {
        // Bits [0,7] -- full byte 0
        ShaderVariantKey key = {};
        key.SetValue(0, 7, 0xAB);
        ASSERT_EQ(key.GetValue(0, 7), 0xAB);
    }
    {
        // Bits [16,19] -- 4-bit value within byte 2
        ShaderVariantKey key = {};
        key.SetValue(16, 19, 9);
        ASSERT_EQ(key.GetValue(16, 19), 9);
    }
}

// --- SetValue / GetValue: crossing a byte boundary ---
TEST(ShaderTest, SetGetCrossByteBoundary)
{
    {
        // Bits [5,10] -- starts in byte 0, ends in byte 1 (6 bits)
        ShaderVariantKey key = {};
        key.SetValue(5, 10, 0x3F); // max 6-bit value
        ASSERT_EQ(key.GetValue(5, 10), 0x3F);
    }
    {
        // Bits [6,9] -- 4 bits crossing byte 0 and byte 1
        ShaderVariantKey key = {};
        key.SetValue(6, 9, 0x0A);
        ASSERT_EQ(key.GetValue(6, 9), 0x0A);
    }
    {
        // Bits [12,17] -- crossing byte 1 and byte 2
        ShaderVariantKey key = {};
        key.SetValue(12, 17, 42);
        ASSERT_EQ(key.GetValue(12, 17), 42);
    }
    {
        // Bits [28,35] -- crossing byte 3 and byte 4 (8 bits, original test value)
        ShaderVariantKey key = {};
        key.SetValue(28, 35, 127);
        ASSERT_EQ(key.GetValue(28, 35), 127);
    }
}

// --- SetValue / GetValue: high bits near the end of the key (56..63) ---
TEST(ShaderTest, SetGetHighBits)
{
    {
        // Single bit at bit 63 (the very last bit)
        ShaderVariantKey key = {};
        key.SetValue(63, 63, 1);
        ASSERT_EQ(key.GetValue(63, 63), 1);
        // Lower bits should be unaffected
        ASSERT_EQ(key.GetValue(0, 7), 0);
    }
    {
        // Bits [56,63] -- full last byte
        ShaderVariantKey key = {};
        key.SetValue(56, 63, 0xCD);
        ASSERT_EQ(key.GetValue(56, 63), 0xCD);
        ASSERT_EQ(key.GetValue(0, 7), 0);
    }
    {
        // Bits [60,63] -- upper nibble of last byte
        ShaderVariantKey key = {};
        key.SetValue(60, 63, 0x0F);
        ASSERT_EQ(key.GetValue(60, 63), 0x0F);
    }
    {
        // Bits [56,59] -- lower nibble of last byte
        ShaderVariantKey key = {};
        key.SetValue(56, 59, 0x05);
        ASSERT_EQ(key.GetValue(56, 59), 0x05);
    }
}

// --- SetValue / GetValue: crossing into the last byte (byte 6->7) ---
TEST(ShaderTest, SetGetCrossIntoLastByte)
{
    {
        // Bits [53,60] -- 8 bits crossing byte 6 and byte 7
        ShaderVariantKey key = {};
        key.SetValue(53, 60, 0xA5);
        ASSERT_EQ(key.GetValue(53, 60), 0xA5);
    }
    {
        // Bits [50,55] -- 6 bits crossing byte 6 and byte 6/7 boundary
        ShaderVariantKey key = {};
        key.SetValue(50, 55, 0x2B);
        ASSERT_EQ(key.GetValue(50, 55), 0x2B);
    }
}

// --- Multiple non-overlapping fields should not corrupt each other ---
TEST(ShaderTest, SetGetMultipleFieldsIsolation)
{
    ShaderVariantKey key = {};

    key.SetValue(0, 0, 1);        // bit 0
    key.SetValue(1, 3, 5);        // bits 1-3
    key.SetValue(8, 15, 0xFF);    // bits 8-15 (full byte 1)
    key.SetValue(32, 39, 0xAB);   // bits 32-39 (full byte 4)
    key.SetValue(60, 63, 0x0C);   // bits 60-63

    ASSERT_EQ(key.GetValue(0, 0), 1);
    ASSERT_EQ(key.GetValue(1, 3), 5);
    ASSERT_EQ(key.GetValue(8, 15), 0xFF);
    ASSERT_EQ(key.GetValue(32, 39), 0xAB);
    ASSERT_EQ(key.GetValue(60, 63), 0x0C);

    // Unset regions should be 0
    ASSERT_EQ(key.GetValue(16, 23), 0);
    ASSERT_EQ(key.GetValue(40, 47), 0);
}

// --- Overwrite: setting a field twice should update correctly ---
TEST(ShaderTest, SetValueOverwrite)
{
    ShaderVariantKey key = {};

    key.SetValue(4, 7, 0x0F);
    ASSERT_EQ(key.GetValue(4, 7), 0x0F);

    key.SetValue(4, 7, 0x03);
    ASSERT_EQ(key.GetValue(4, 7), 0x03);

    // Overwrite with 0
    key.SetValue(4, 7, 0x00);
    ASSERT_EQ(key.GetValue(4, 7), 0x00);
}

// --- Overwrite should not affect adjacent bits ---
TEST(ShaderTest, SetValueOverwriteIsolation)
{
    ShaderVariantKey key = {};

    key.SetValue(0, 3, 0x0F);
    key.SetValue(4, 7, 0x0A);

    // Overwrite bits [0,3]
    key.SetValue(0, 3, 0x05);
    ASSERT_EQ(key.GetValue(0, 3), 0x05);
    ASSERT_EQ(key.GetValue(4, 7), 0x0A); // adjacent should be unchanged
}

// --- Max value for each bit width (1..7 bits) ---
TEST(ShaderTest, SetGetMaxValues)
{
    for (uint8_t bits = 1; bits <= 7; ++bits) {
        uint8_t maxVal = static_cast<uint8_t>((1u << bits) - 1);
        ShaderVariantKey key = {};
        key.SetValue(0, bits - 1, maxVal);
        ASSERT_EQ(key.GetValue(0, bits - 1), maxVal) << "bits=" << (int)bits;
    }
}

// --- GenerateDefault should apply default values to all entries ---
TEST(ShaderTest, GenerateDefaultWithNonZeroDefaults)
{
    ShaderVariantList list;

    // entry at bits [0,0] with default=1
    list.AddEntry({Name("flag_a"), {0, 0}, 1});
    // entry at bits [1,3] with default=5
    list.AddEntry({Name("mode_b"), {1, 3}, 5});
    // entry at bits [8,15] with default=0xAB
    list.AddEntry({Name("param_c"), {8, 15}, 0xAB});

    ShaderVariantKey dflt = list.GenerateDefault();

    ASSERT_EQ(dflt.GetValue(0, 0), 1);
    ASSERT_EQ(dflt.GetValue(1, 3), 5);
    ASSERT_EQ(dflt.GetValue(8, 15), 0xAB);
    // Bits not covered by any entry should remain 0
    ASSERT_EQ(dflt.GetValue(16, 23), 0);
}

// --- Zero-init: freshly constructed key should read 0 everywhere ---
TEST(ShaderTest, ZeroInitialized)
{
    ShaderVariantKey key = {};
    for (uint8_t byte = 0; byte < 8; ++byte) {
        ASSERT_EQ(key.GetValue(byte * 8, byte * 8 + 7), 0) << "byte=" << (int)byte;
    }
}

TEST(ShaderTest, ShaderVariantListTest1)
{
    ShaderVariantList list;

    list.AddEntry({Name("a"), {0, 0}});
    list.AddEntry({Name("b"), {1, 1}});

    auto permutations = list.GeneratePermutation();
    ASSERT_EQ(permutations.size(), 4);

}

TEST(ShaderTest, ShaderVariantListTest2)
{
    ShaderVariantList list;

    list.AddEntry({Name("a"), {0, 0}});
    list.AddEntry({Name("b"), {1, 2}});
    list.AddEntry({Name("c"), {3, 5}});
    list.AddEntry({Name("e"), {11, 11}});

    list.AddDependency(Name("c"), ShaderOptionDependency{Name("a"), ShaderOptionFunc::EQ, 1});
    list.AddDependency(Name("b"), ShaderOptionDependency{Name("c"), ShaderOptionFunc::EQ, 3});

    auto permutations = list.GeneratePermutation();
    ASSERT_EQ(permutations.size(), 8);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}