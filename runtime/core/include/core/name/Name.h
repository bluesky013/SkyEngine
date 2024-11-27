//
// Created by blues on 2024/11/22.
//

#pragma once

#include <core/name/NameTypes.h>

namespace sky {

    class Name {
    public:
        Name();
        explicit Name(const char* ch);
        ~Name() noexcept = default;

        static uint32_t Hash(const char* ch, uint32_t length) noexcept;

        std::string_view GetStr() const noexcept;

        static bool Equals(Name A, std::string_view B) noexcept;
        static bool Equals(Name A, const char* B) noexcept;
        static bool Equals(Name A, Name B) noexcept;

        inline bool operator==(const Name &other) const noexcept
        {
            return handle == other.handle;
        }

        template <typename T>
        friend inline auto operator==(Name N, T O) -> decltype(Name::Equals(N, O))
        {
            return Name::Equals(N, O);
        }

        template <typename T>
        friend inline auto operator==(T O, Name N) -> decltype(Name::Equals(N, O))
        {
            return Name::Equals(N, O);
        }

        template <typename Stream>
        friend inline Stream &operator << (Stream &os, const Name& dt)
        {
            os << dt.GetStr();
            return os;
        }

        inline bool operator < (const Name &other) const noexcept
        {
            return handle < other.handle;
        }

        inline bool operator > (const Name &other) const noexcept
        {
            return handle > other.handle;
        }

        inline bool operator != (const Name &other) const noexcept
        {
            return handle != other.handle;
        }

        NameEntryHandle GetHandle() const noexcept
        {
            return handle;
        }

        bool Empty() const noexcept
        {
            return handle == 0;
        }
    private:
        NameEntryHandle handle;
    };

} // namespace sky

namespace std {

    template <>
    struct hash<sky::Name> {
        size_t operator()(const sky::Name &nane) const noexcept
        {
            return nane.GetHandle();
        }
    };

    template <>
    struct equal_to<sky::Name> {
        bool operator()(const sky::Name &x, const sky::Name &y) const noexcept
        {
            return x == y;
        }
    };

} // namespace std
