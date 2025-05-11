//
// Created by Zach Lee on 2023/4/2.
//

#include <core/util/String.h>
#include <codecvt>

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


    std::wstring Utf8ToUtf16(const std::string &str)
    {
//        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
//        return converter.from_bytes(str);
        // TODO
        return {};
    }

//    bool ConvertUTF8toUTF16(const UTF8** sourceStart, const UTF8* sourceEnd, UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags)
//    {
//        const UTF8* source = *sourceStart;
//        UTF16* target = *targetStart;
//        while (source < sourceEnd) {
//            UTF32 ch = 0;
//            unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
//            if (extraBytesToRead >= sourceEnd - source) {
//                result = sourceExhausted; break;
//            }
//            /* Do this check whether lenient or strict */
//            if (!isLegalUTF8(source, extraBytesToRead+1)) {
//                result = sourceIllegal;
//                break;
//            }
//            /*
//             * The cases all fall through. See "Note A" below.
//             */
//            switch (extraBytesToRead) {
//                case 5: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
//                case 4: ch += *source++; ch <<= 6; /* remember, illegal UTF-8 */
//                case 3: ch += *source++; ch <<= 6;
//                case 2: ch += *source++; ch <<= 6;
//                case 1: ch += *source++; ch <<= 6;
//                case 0: ch += *source++;
//            }
//            ch -= offsetsFromUTF8[extraBytesToRead];
//
//            if (target >= targetEnd) {
//                source -= (extraBytesToRead+1); /* Back up source pointer! */
//                result = targetExhausted; break;
//            }
//            if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
//                /* UTF-16 surrogate values are illegal in UTF-32 */
//                if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
//                    if (flags == strictConversion) {
//                        source -= (extraBytesToRead+1); /* return to the illegal value itself */
//                        result = sourceIllegal;
//                        break;
//                    } else {
//                        *target++ = UNI_REPLACEMENT_CHAR;
//                    }
//                } else {
//                    *target++ = (UTF16)ch; /* normal case */
//                }
//            } else if (ch > UNI_MAX_UTF16) {
//                if (flags == strictConversion) {
//                    result = sourceIllegal;
//                    source -= (extraBytesToRead+1); /* return to the start */
//                    break; /* Bail out; shouldn't continue */
//                } else {
//                    *target++ = UNI_REPLACEMENT_CHAR;
//                }
//            } else {
//                /* target is a character in range 0xFFFF - 0x10FFFF. */
//                if (target + 1 >= targetEnd) {
//                    source -= (extraBytesToRead+1); /* Back up source pointer! */
//                    result = targetExhausted; break;
//                }
//                ch -= halfBase;
//                *target++ = (UTF16)((ch >> halfShift) + UNI_SUR_HIGH_START);
//                *target++ = (UTF16)((ch & halfMask) + UNI_SUR_LOW_START);
//            }
//        }
//        *sourceStart = source;
//        *targetStart = target;
//        return result;
//    }
}
