//
// Created by Zach Lee on 2023/4/2.
//

#include <core/util/String.h>
#include <cstdint>

namespace sky {

    const std::string &GetEmpty()
    {
        static std::string empty;
        return empty;
    }

    std::vector<std::string> Split(const std::string &s, const char *separator)
    {
        std::vector<std::string> output;
        std::string::size_type prev = 0, pos = 0;
        while ((pos = s.find_first_of(std::string(separator), pos)) != std::string::npos) {
            if (pos > prev) {
                std::string substring(s.substr(prev, pos - prev));
                output.push_back(substring);
            }
            prev = ++pos;
        }

        if (prev <= s.length()) {
            output.push_back(s.substr(prev, pos - prev));
        }
        return output;
    }


    // Decode one UTF-8 code point from [it, end). Advances it past consumed bytes.
    // Returns 0xFFFD (replacement char) on invalid input.
    static uint32_t DecodeUtf8(const char *&it, const char *end)
    {
        auto byte = static_cast<uint8_t>(*it);
        uint32_t cp;
        int extra;

        if (byte < 0x80) {
            cp = byte; extra = 0;
        } else if ((byte & 0xE0) == 0xC0) {
            cp = byte & 0x1F; extra = 1;
        } else if ((byte & 0xF0) == 0xE0) {
            cp = byte & 0x0F; extra = 2;
        } else if ((byte & 0xF8) == 0xF0) {
            cp = byte & 0x07; extra = 3;
        } else {
            ++it;
            return 0xFFFD;
        }

        ++it;
        for (int i = 0; i < extra; ++i) {
            if (it == end || (static_cast<uint8_t>(*it) & 0xC0) != 0x80) {
                return 0xFFFD;
            }
            cp = (cp << 6) | (static_cast<uint8_t>(*it) & 0x3F);
            ++it;
        }

        // Overlong / surrogate / out-of-range checks
        if (cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF) ||
            (extra == 1 && cp < 0x80) ||
            (extra == 2 && cp < 0x800) ||
            (extra == 3 && cp < 0x10000)) {
            return 0xFFFD;
        }
        return cp;
    }

    // Encode a Unicode code point as UTF-8, appending to out.
    static void EncodeUtf8(uint32_t cp, std::string &out)
    {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    // Encode a code point into UTF-16 code units, appending to a container via push_back.
    template <typename Container>
    static void EncodeUtf16(uint32_t cp, Container &out)
    {
        using ValueType = typename Container::value_type;
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<ValueType>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<ValueType>((cp >> 10) + 0xD800));
            out.push_back(static_cast<ValueType>((cp & 0x3FF) + 0xDC00));
        }
    }

    // Decode one UTF-16 code point from [it, end). Advances it past consumed units.
    template <typename It>
    static uint32_t DecodeUtf16(It &it, It end)
    {
        auto unit = static_cast<uint32_t>(*it);
        ++it;
        if (unit >= 0xD800 && unit <= 0xDBFF) { // high surrogate
            if (it != end) {
                auto low = static_cast<uint32_t>(*it);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    ++it;
                    return ((unit - 0xD800) << 10) + (low - 0xDC00) + 0x10000;
                }
            }
            return 0xFFFD; // unpaired high surrogate
        }
        if (unit >= 0xDC00 && unit <= 0xDFFF) {
            return 0xFFFD; // unpaired low surrogate
        }
        return unit;
    }

    std::wstring Utf8ToUtf16(const std::string &str)
    {
        std::wstring result;
        result.reserve(str.size());
        const char *it = str.data();
        const char *end = it + str.size();
        while (it < end) {
            uint32_t cp = DecodeUtf8(it, end);
            EncodeUtf16(cp, result);
        }
        return result;
    }

    std::u16string Utf8ToUtf16U(const std::string &str)
    {
        std::u16string result;
        result.reserve(str.size());
        const char *it = str.data();
        const char *end = it + str.size();
        while (it < end) {
            uint32_t cp = DecodeUtf8(it, end);
            EncodeUtf16(cp, result);
        }
        return result;
    }

    std::string Utf16ToUtf8(const std::wstring &str)
    {
        std::string result;
        result.reserve(str.size() * 2);
        auto it = str.begin();
        auto end = str.end();
        while (it != end) {
            uint32_t cp = DecodeUtf16(it, end);
            EncodeUtf8(cp, result);
        }
        return result;
    }

    std::string Utf16ToUtf8(const std::u16string &str)
    {
        std::string result;
        result.reserve(str.size() * 2);
        auto it = str.begin();
        auto end = str.end();
        while (it != end) {
            uint32_t cp = DecodeUtf16(it, end);
            EncodeUtf8(cp, result);
        }
        return result;
    }
}
