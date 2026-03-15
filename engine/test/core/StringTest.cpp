//
// Created by Copilot on 2026/3/15.
//

#include <core/util/String.h>
#include <gtest/gtest.h>

using namespace sky;

// --- Utf8ToUtf16 / Utf16ToUtf8 round-trip ---

TEST(StringTest, Utf8ToUtf16_Ascii)
{
    std::string src = "Hello, World!";
    std::wstring wide = Utf8ToUtf16(src);
    ASSERT_EQ(wide, L"Hello, World!");
}

TEST(StringTest, Utf16ToUtf8_Ascii)
{
    std::wstring src = L"Hello, World!";
    std::string utf8 = Utf16ToUtf8(src);
    ASSERT_EQ(utf8, "Hello, World!");
}

TEST(StringTest, Utf8ToUtf16_Empty)
{
    ASSERT_TRUE(Utf8ToUtf16("").empty());
    ASSERT_TRUE(Utf16ToUtf8(std::wstring{}).empty());
}

// --- CJK (3-byte UTF-8) ---

TEST(StringTest, Utf8ToUtf16_CJK)
{
    // "你好" in UTF-8 is 6 bytes (0xE4BDA0 0xE5A5BD)
    std::string src = "\xE4\xBD\xA0\xE5\xA5\xBD";
    std::wstring wide = Utf8ToUtf16(src);
    ASSERT_EQ(wide.size(), 2u);
    ASSERT_EQ(wide[0], L'\u4F60'); // 你
    ASSERT_EQ(wide[1], L'\u597D'); // 好
}

TEST(StringTest, Utf8Utf16_CJK_RoundTrip)
{
    std::string src = "\xE4\xBD\xA0\xE5\xA5\xBD";
    ASSERT_EQ(Utf16ToUtf8(Utf8ToUtf16(src)), src);
}

// --- 2-byte UTF-8 (Latin extended, Cyrillic, etc.) ---

TEST(StringTest, Utf8ToUtf16_TwoByte)
{
    // "café" — 'é' is U+00E9, encoded as 0xC3 0xA9
    std::string src = "caf\xC3\xA9";
    std::wstring wide = Utf8ToUtf16(src);
    ASSERT_EQ(wide.size(), 4u);
    ASSERT_EQ(wide[3], L'\u00E9');

    ASSERT_EQ(Utf16ToUtf8(wide), src);
}

// --- 4-byte UTF-8 / surrogate pairs (Supplementary Plane) ---

TEST(StringTest, Utf8ToUtf16_Surrogate)
{
    // U+1F600 (😀) in UTF-8: F0 9F 98 80
    std::string src = "\xF0\x9F\x98\x80";
    std::wstring wide = Utf8ToUtf16(src);

    // On Windows wchar_t is 16-bit, so supplementary chars become a surrogate pair
    ASSERT_EQ(wide.size(), 2u);
    ASSERT_EQ(static_cast<uint32_t>(wide[0]), 0xD83D); // high surrogate
    ASSERT_EQ(static_cast<uint32_t>(wide[1]), 0xDE00); // low surrogate

    ASSERT_EQ(Utf16ToUtf8(wide), src);
}

// --- u16string variants ---

TEST(StringTest, Utf8ToUtf16U_Ascii)
{
    std::string src = "test";
    std::u16string u16 = Utf8ToUtf16U(src);
    ASSERT_EQ(u16.size(), 4u);
    ASSERT_EQ(u16[0], u't');
    ASSERT_EQ(u16[3], u't');
}

TEST(StringTest, Utf8ToUtf16U_CJK)
{
    std::string src = "\xE4\xBD\xA0\xE5\xA5\xBD";
    std::u16string u16 = Utf8ToUtf16U(src);
    ASSERT_EQ(u16.size(), 2u);
    ASSERT_EQ(u16[0], u'\u4F60');
    ASSERT_EQ(u16[1], u'\u597D');
}

TEST(StringTest, Utf16ToUtf8_u16string_RoundTrip)
{
    std::string src = "\xF0\x9F\x98\x80"; // U+1F600
    std::u16string u16 = Utf8ToUtf16U(src);
    ASSERT_EQ(u16.size(), 2u); // surrogate pair
    ASSERT_EQ(Utf16ToUtf8(u16), src);
}

// --- Invalid / edge cases ---

TEST(StringTest, Utf8ToUtf16_InvalidContinuationByte)
{
    // 0x80 alone is invalid (continuation byte without lead)
    std::string src = "\x80";
    std::wstring wide = Utf8ToUtf16(src);
    ASSERT_EQ(wide.size(), 1u);
    ASSERT_EQ(static_cast<uint32_t>(wide[0]), 0xFFFD);
}

TEST(StringTest, Utf8ToUtf16_TruncatedSequence)
{
    // 0xC3 expects one more continuation byte but string ends
    std::string src = "\xC3";
    std::wstring wide = Utf8ToUtf16(src);
    ASSERT_EQ(wide.size(), 1u);
    ASSERT_EQ(static_cast<uint32_t>(wide[0]), 0xFFFD);
}

TEST(StringTest, Utf8ToUtf16_OverlongTwoByte)
{
    // Overlong encoding of U+0000: C0 80 (should be rejected)
    std::string src = "\xC0\x80";
    std::wstring wide = Utf8ToUtf16(src);
    ASSERT_EQ(wide.size(), 1u);
    ASSERT_EQ(static_cast<uint32_t>(wide[0]), 0xFFFD);
}

TEST(StringTest, Utf16ToUtf8_UnpairedHighSurrogate)
{
    // High surrogate 0xD800 without a following low surrogate
    std::u16string src = {char16_t(0xD800), u'A'};
    std::string utf8 = Utf16ToUtf8(src);
    // Should produce replacement char for unpaired surrogate, then 'A'
    ASSERT_FALSE(utf8.empty());
    // U+FFFD in UTF-8 is 0xEF 0xBF 0xBD
    ASSERT_EQ(static_cast<uint8_t>(utf8[0]), 0xEF);
    ASSERT_EQ(static_cast<uint8_t>(utf8[1]), 0xBF);
    ASSERT_EQ(static_cast<uint8_t>(utf8[2]), 0xBD);
    ASSERT_EQ(utf8[3], 'A');
}

TEST(StringTest, Utf16ToUtf8_UnpairedLowSurrogate)
{
    // Low surrogate 0xDC00 alone is invalid
    std::u16string src = {char16_t(0xDC00)};
    std::string utf8 = Utf16ToUtf8(src);
    ASSERT_EQ(static_cast<uint8_t>(utf8[0]), 0xEF);
    ASSERT_EQ(static_cast<uint8_t>(utf8[1]), 0xBF);
    ASSERT_EQ(static_cast<uint8_t>(utf8[2]), 0xBD);
}

// --- Split ---

TEST(StringTest, Split_Basic)
{
    auto result = Split("hello world foo", " ");
    ASSERT_EQ(result.size(), 3u);
    ASSERT_EQ(result[0], "hello");
    ASSERT_EQ(result[1], "world");
    ASSERT_EQ(result[2], "foo");
}

TEST(StringTest, Split_MultipleSeparators)
{
    auto result = Split("a,b;c", ",;");
    ASSERT_EQ(result.size(), 3u);
    ASSERT_EQ(result[0], "a");
    ASSERT_EQ(result[1], "b");
    ASSERT_EQ(result[2], "c");
}

TEST(StringTest, Split_NoSeparator)
{
    auto result = Split("hello", ",");
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0], "hello");
}

// --- GetEmpty ---

TEST(StringTest, GetEmpty)
{
    const std::string &empty = String::GetEmpty();
    ASSERT_TRUE(empty.empty());
}
