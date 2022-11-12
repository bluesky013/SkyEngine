//
// Created by Zach Lee on 2022/11/12.
//

#pragma once

#include <type_traits>

namespace sky {

    template <typename T>
    class Flags {
    public:
        using ValueType = typename std::underlying_type<T>::type;

        constexpr Flags() noexcept : value{0}
        {
        }

        constexpr Flags(T bit) noexcept : value(static_cast<ValueType>(bit))
        {
        }

        constexpr Flags(Flags<T> const& rhs) noexcept : value(rhs.value)
        {
        }

        constexpr explicit Flags(ValueType flags) noexcept : value(flags)
        {
        }

        constexpr bool operator==(Flags<T> const& rhs) const noexcept
        {
            return value == rhs.value;
        }

        constexpr bool operator!=(Flags<T> const& rhs) const noexcept
        {
            return value != rhs.value;
        }

        constexpr Flags<T> operator&(Flags<T> const& rhs) const noexcept
        {
            return Flags<T>(value & rhs.value);
        }

        constexpr Flags<T> operator|(Flags<T> const& rhs) const noexcept
        {
            return Flags<T>(value | rhs.value);
        }

        constexpr Flags<T> operator^(Flags<T> const& rhs) const noexcept
        {
            return Flags<T>(value ^ rhs.value);
        }

        // assignment operators
        constexpr Flags<T> & operator=(Flags<T> const& rhs) noexcept
        {
            value = rhs.value;
            return *this;
        }

        constexpr Flags<T> & operator|=(Flags<T> const& rhs) noexcept
        {
            value |= rhs.value;
            return *this;
        }

        constexpr Flags<T> & operator&=(Flags<T> const& rhs) noexcept
        {
            value &= rhs.value;
            return *this;
        }

        constexpr Flags<T> & operator^=(Flags<T> const& rhs) noexcept
        {
            value ^= rhs.value;
            return *this;
        }

        explicit constexpr operator bool() const noexcept
        {
            return value != 0;
        }

        ValueType value;
    };

    template <typename T>
    constexpr Flags<T> operator&(T bit, Flags<T> const& flags) noexcept
    {
        return flags.operator&( bit );
    }

    template <typename T>
    constexpr Flags<T> operator|(T bit, Flags<T> const& flags) noexcept
    {
        return flags.operator|( bit );
    }

    template <typename T>
    constexpr Flags<T> operator^(T bit, Flags<T> const& flags) noexcept
    {
        return flags.operator^( bit );
    }

}