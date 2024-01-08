//
// Created by bluesky on 2023/10/11.
//

#pragma once

#include <iostream>

namespace sky {

    class IStreamArchive {
    public:
        explicit IStreamArchive(std::istream &s) : stream(s) {}
        ~IStreamArchive() = default;

        bool Load(char *data, uint32_t size);

        template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        bool Load(T &val)
        {
            return Load(reinterpret_cast<char *>(&val), sizeof(T));
        }

        template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        IStreamArchive &operator>>(T &v)
        {
            Load(v);
            return *this;
        }

        bool LoadString(std::string &str);

    private:
        std::istream &stream;
    };

    class OStreamArchive {
    public:
        explicit OStreamArchive(std::ostream &s) : stream(s) {}
        ~OStreamArchive() = default;

        bool Save(const char *data, uint32_t size);

        template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        bool Save(const T &v)
        {
            return Save(reinterpret_cast<const char *>(&v), sizeof(T));
        }

        template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        OStreamArchive &operator<<(const T &v)
        {
            Save(v);
            return *this;
        }

        bool SaveString(const std::string &str);

    private:
        std::ostream &stream;
    };

} // namespace sky