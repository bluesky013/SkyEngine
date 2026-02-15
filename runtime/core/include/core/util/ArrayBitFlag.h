//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <array>
#include <bit>
#include <type_traits>
#include <core/hash/Hash.h>

namespace sky {

    template <typename T, uint32_t MAX>
    class ArrayBit {
    public:
        static constexpr uint32_t STEP = 32;
        static constexpr uint32_t NUM = (MAX / STEP) + 1;

        struct MaskFull {};

        ArrayBit()
        {
            for (auto &v : values) {
                v = 0;
            }
        }

        ArrayBit(const MaskFull &tag)
        {
            for (auto& v : values) {
                v = std::numeric_limits<uint32_t>::max();
            }
        }

        ArrayBit(const T& v) : ArrayBit()
        {
            SetBit(v);
        }

        ~ArrayBit() = default;

        ArrayBit(const ArrayBit &val) = default;
        ArrayBit &operator=(const ArrayBit &val) = default;

        bool AnyBit() const noexcept
        {
            for (const auto &v : values) {
                if (v != 0) {
                    return true;
                }
            }
            return false;
        }

        uint32_t Count() const noexcept
        {
            uint32_t count = 0;
            for (const auto &v : values) {
                count += std::popcount(v); // C++20
            }
            return count;
        }

        void SetBit(const T &v) noexcept
        {
            const auto val = static_cast<uint32_t>(v);
            uint32_t index = val / STEP;
            const uint32_t offset = val % STEP;
            values[index] |= (1U << offset);
        }

        void ResetBit(const T &v) noexcept
        {
            const auto val = static_cast<uint32_t>(v);
            uint32_t index = val / STEP;
            const uint32_t offset = val % STEP;
            values[index] &= ~(1U << offset);
        }

        bool CheckBit(const T &v) const noexcept
        {
            const auto val = static_cast<uint32_t>(v);
            uint32_t index = val / STEP;
            const uint32_t offset = val % STEP;
            return (values[index] & (1U << offset)) != 0;
        }

        ArrayBit &operator&=(const ArrayBit &val) noexcept
        {
            for (uint32_t i = 0; i < NUM; ++i) {
                values[i] &= val.values[i];
            }
            return *this;
        }

        ArrayBit &operator|=(const ArrayBit &val) noexcept
        {
            for (uint32_t i = 0; i < NUM; ++i) {
                values[i] |= val.values[i];
            }
            return *this;
        }

        ArrayBit<T, MAX> operator&(const ArrayBit &val) const noexcept
        {
            return ArrayBit(*this) &= val;
        }

        ArrayBit<T, MAX> operator|(const ArrayBit &val) const noexcept
        {
            return ArrayBit(*this) |= val;
        }


        bool operator==(const ArrayBit& val) const noexcept {
            return memcmp(&values[0], &val.values[0], sizeof(values)) == 0;
        }

        std::array<uint32_t, NUM> values;
    };
}

namespace std {

    template <typename T, uint32_t N>
    struct hash<sky::ArrayBit<T, N>> {
    size_t operator()(const sky::ArrayBit<T, N> &flags) const noexcept
    {
        uint32_t hash = 0;
        for (const auto &val : flags.values) {
            sky::HashCombine32(hash, val);
        }
        return hash;
    }
};

template <typename T, uint32_t N>
struct equal_to<sky::ArrayBit<T, N>> {
    bool operator()(const sky::ArrayBit<T, N> &x, const sky::ArrayBit<T, N> &y) const noexcept
    {
        return x == y;
    }
};

} // namespace std
