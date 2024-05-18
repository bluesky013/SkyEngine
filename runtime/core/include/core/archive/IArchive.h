//
// Created by blues on 2024/5/16.
//

#pragma once

#include <cstdint>
#include <memory>
#include <core/archive/Concept.h>

namespace sky {

    class IInputArchive {
    public:
        IInputArchive() = default;
        virtual ~IInputArchive() = default;

        virtual bool LoadRaw(char *data, size_t size) = 0;

        template <ContainerDataType T>
        bool Load(T &v)
        {
            uint32_t size = 0;
            bool res = true;
            res &= Load(size);
            v.resize(size / sizeof(typename T::value_type));
            res &= LoadRaw(reinterpret_cast<char*>(v.data()), size);
            return res;
        }

        template <ArithmeticDataType T>
        bool Load(T &val)
        {
            return LoadRaw(reinterpret_cast<char *>(&val), sizeof(T));
        }

        template <ArithmeticDataType T>
        IInputArchive &operator>>(T &v)
        {
            Load(v);
            return *this;
        }
    };
    using IArchivePtr = std::shared_ptr<IInputArchive>;

    class IOutputArchive {
    public:
        IOutputArchive() = default;
        virtual ~IOutputArchive() = default;

        virtual bool SaveRaw(const char *data, size_t size) = 0;

        template <ContainerDataType T>
        bool Save(const T &v)
        {
            auto size = v.size() * sizeof(typename T::value_type);
            bool res = true;
            res &= Save(static_cast<uint32_t>(size));
            res &= SaveRaw(reinterpret_cast<const char *>(v.data()), size);
            return res;
        }

        template <ArithmeticDataType T>
        bool Save(const T &v)
        {
            return SaveRaw(reinterpret_cast<const char *>(&v), sizeof(T));
        }

        template <ArithmeticDataType T>
        IOutputArchive &operator<<(const T &v)
        {
            Save(v);
            return *this;
        }
    };
    using OArchivePtr = std::shared_ptr<IOutputArchive>;

} // namespace sky
