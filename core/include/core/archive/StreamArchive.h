//
// Created by bluesky on 2023/10/11.
//

#pragma once

#include <iostream>
#include <core/archive/ArchiveConcept.h>

namespace sky {

    class IStreamArchive {
    public:
        explicit IStreamArchive(std::istream &s) : stream(s) {}
        ~IStreamArchive() = default;

        bool Load(char *data, size_t size);

        template <ContainerDataType T>
        bool Load(T &v)
        {
            uint32_t size = 0;
            bool res = true;
            res &= Load(size);
            v.resize(size / sizeof(typename T::value_type));
            res &= Load(reinterpret_cast<char*>(v.data()), size);
            return res;
        }

        template <ArithmeticDataType T>
        bool Load(T &val)
        {
            return Load(reinterpret_cast<char *>(&val), sizeof(T));
        }

        template <ArithmeticDataType T>
        IStreamArchive &operator>>(T &v)
        {
            Load(v);
            return *this;
        }

    private:
        std::istream &stream;
    };

    class OStreamArchive {
    public:
        explicit OStreamArchive(std::ostream &s) : stream(s) {}
        ~OStreamArchive() = default;

        bool Save(const char *data, size_t size);

        template <ContainerDataType T>
        bool Save(const T &v)
        {
            auto size = v.size() * sizeof(typename T::value_type);
            bool res = true;
            res &= Save(static_cast<uint32_t>(size));
            res &= Save(reinterpret_cast<const char *>(v.data()), size);
            return res;
        }

        template <ArithmeticDataType T>
        bool Save(const T &v)
        {
            return Save(reinterpret_cast<const char *>(&v), sizeof(T));
        }

        template <ArithmeticDataType T>
        OStreamArchive &operator<<(const T &v)
        {
            Save(v);
            return *this;
        }

    private:
        std::ostream &stream;
    };

} // namespace sky