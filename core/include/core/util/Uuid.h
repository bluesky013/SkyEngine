//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sky {

    static constexpr std::string_view DIGITS = "0123456789ABCDEFabcdef";
    constexpr size_t                  GetDigit(char ch)
    {
        auto it = DIGITS.find(ch);
        if (it == std::string::npos) {
            return it;
        }
        if (it >= 16) {
            it -= 16;
        }
        return it;
    }

    /**
     * Version 4
     * RFC_4122
     */
    class Uuid {
    public:
        constexpr Uuid() : data{0}
        {
        }

        ~Uuid() = default;

        static Uuid Create();

        static Uuid CreateWithSeed(uint32_t);

        /**
         * create uuid from string view
         * @param str "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" format
         * @return uuid
         */
        static constexpr Uuid CreateFromString(std::string_view str)
        {
            Uuid res;
            if (str.size() != 36) {
                return Uuid();
            }

            uint32_t index = 0;
            for (uint32_t i = 0; i < 36; i++) {
                if (i == 8 || i == 13 || i == 18 || i == 23) {
                    continue;
                }
                auto c1 = GetDigit(str[i++]);
                auto c2 = GetDigit(str[i]);
                if (c1 == std::string_view::npos || c2 == std::string_view::npos) {
                    return Uuid();
                }
                res.data[index] = (c1 << 4) & 0xF0;
                res.data[index++] |= (c2 & 0x0F);
            }
            return res;
        }

        std::string ToString() const;

        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(word[0], word[1]);
        }

    private:
        friend struct std::hash<sky::Uuid>;
        friend struct std::equal_to<sky::Uuid>;
        friend bool operator==(const Uuid &lhs, const Uuid &rhs);

        union {
            uint64_t word[2];
            uint8_t  data[16];
        };
    };

    inline bool operator==(const Uuid &lhs, const Uuid &rhs)
    {
        return lhs.word[0] == rhs.word[0] && lhs.word[1] == rhs.word[1];
    }
} // namespace sky

namespace std {

    template <>
    struct hash<sky::Uuid> {
        size_t operator()(const sky::Uuid &uuid) const noexcept
        {
            return uuid.word[0];
        }
    };

    template <>
    struct equal_to<sky::Uuid> {
        bool operator()(const sky::Uuid &x, const sky::Uuid &y) const noexcept
        {
            return (x.word[0] == y.word[0]) && (x.word[1] == y.word[1]);
        }
    };

} // namespace std
