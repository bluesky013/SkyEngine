//
// Created by Zach Lee on 2023/1/14.
//

#pragma once

#include <array>
#include <type_traits>

namespace sky {

    template <typename T, uint32_t MAX>
    class ArrayBit {
    public:
        static constexpr uint32_t STEP = 32;
        static constexpr uint32_t NUM = MAX / STEP + 1;

        ArrayBit()
        {
            for (auto &v : values) {
                v = 0;
            }
        }

        ArrayBit(const T& v) : ArrayBit()
        {
            SetBit(v);
        }

        ~ArrayBit() = default;

        ArrayBit(const ArrayBit &val)
        {
            for (uint32_t i = 0; i < NUM; ++i) {
                values[i] = val.values[i];
            }
        }

        ArrayBit &operator=(const ArrayBit &val)
        {
            for (uint32_t i = 0; i < NUM; ++i) {
                values[i] = val.values[i];
            }
            return *this;
        }

        void SetBit(const T &v) noexcept
        {
            auto val = static_cast<std::underlying_type_t<T>>(v);
            uint32_t index = val / STEP;
            uint32_t offset = val % STEP;
            values[index] |= (1 << offset);
        }

        void ResetBit(const T &v) noexcept
        {
            auto val = static_cast<std::underlying_type_t<T>>(v);
            uint32_t index = val / STEP;
            uint32_t offset = val % STEP;
            values[index] &= ~(1 << offset);
        }

        bool CheckBit(const T &v) noexcept
        {
            auto val = static_cast<std::underlying_type_t<T>>(v);
            uint32_t index = val / STEP;
            uint32_t offset = val % STEP;
            return (values[index] & (1 << offset)) != 0;
        }

        ArrayBit &operator&=(const ArrayBit &val) noexcept
        {
            for (uint32_t i = 0; i < NUM; ++i) {
                values[i] &= val.values[i];
            }
            return *this;
        }

        bool operator==(const ArrayBit& val) const noexcept {
            return memcmp(&values[0], &val.values[0], sizeof(values)) == 0;
        }
    private:
        std::array<uint32_t, NUM> values;
    };

    template <typename T, uint32_t MAX>
    ArrayBit<T, MAX> operator&(const ArrayBit<T, MAX> &lhs, const ArrayBit<T, MAX> &rhs)
    {
        ArrayBit<T, MAX> res = lhs;
        return res &= rhs;
    }
}
