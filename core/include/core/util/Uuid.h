//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace sky {

    /**
     * Version 4
     * RFC_4122
     */
    class Uuid {
    public:
        Uuid();
        ~Uuid() = default;

        static Uuid Create();

        std::string ToString() const;

    private:
        friend struct std::hash<sky::Uuid>;
        friend struct std::equal_to<sky::Uuid>;

        union {
            uint8_t data[16];
            uint64_t word[2];
        };
    };
}

namespace std {

    template <>
    struct hash<sky::Uuid> {
        size_t operator()(const sky::Uuid& uuid) const noexcept
        {
            return uuid.word[0];
        }
    };

    template <>
    struct equal_to<sky::Uuid> {
        bool operator()(const sky::Uuid& x, const sky::Uuid& y) const noexcept
        {
            return x.word[0] == y.word[0] && x.word[1] == y.word[1];
        }
    };

}
