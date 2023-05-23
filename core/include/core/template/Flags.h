//
// Created by Zach Lee on 2022/11/12.
//

#pragma once

#include <type_traits>

namespace sky {

    template <typename T>
    class Flags {
    public:
        using ValueType = typename std::make_unsigned<std::underlying_type_t<T>>::type ;

        static constexpr ValueType AllFlagBits()
        {
            return std::numeric_limits<ValueType>::max();
        }

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

        constexpr Flags<T> operator~() const noexcept
        {
            return Flags<T>(value ^ AllFlagBits());
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

        explicit constexpr operator ValueType() const noexcept
        {
            return value;
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