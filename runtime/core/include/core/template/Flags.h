//
// Created by Zach Lee on 2022/11/12.
//

#pragma once

#include <type_traits>
#include <limits>
#include <map>
#include <functional>
#include <core/template/TypeTraits.h>

namespace sky {

    template <typename T>
    class Flags {
    public:
        using ValueType = std::make_unsigned_t<std::underlying_type_t<T>> ;
        using BinarySerializable = void;

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

        constexpr bool TestBit(T val) const noexcept
        {
            return operator&(val) == val;
        }

        constexpr void SetBit(T val) noexcept
        {
            value |= static_cast<ValueType>(val);
        }

        constexpr void ResetBit(T val) noexcept
        {
            value &= ~static_cast<ValueType>(val);
        }

        constexpr operator bool() const noexcept
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

#define ENABLE_FLAG_BIT_OPERATOR(Type) \
    constexpr Flags<Type> operator&(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) & rhs; \
    } \
    constexpr Flags<Type> operator|(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) | rhs; \
    } \
    constexpr Flags<Type> operator^(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) ^ rhs; \
    }

} // namespace sky

namespace std {

    template <typename T>
    struct hash<sky::Flags<T>> {
        size_t operator()(const sky::Flags<T> &flags) const noexcept
        {
            return static_cast<size_t>(flags.value);
        }
    };

    template <typename T>
    struct equal_to<sky::Flags<T>> {
        bool operator()(const sky::Flags<T> &x, const sky::Flags<T> &y) const noexcept
        {
            return x.value == y.value;
        }
    };

} // namespace std
