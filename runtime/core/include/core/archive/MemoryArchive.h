//
// Created by blues on 2024/1/14.
//

#pragma once

#include <core/archive/ArchiveConcept.h>
#include <core/archive/IArchive.h>
#include <vector>

namespace sky {

    class MemoryArchive {
    public:
        MemoryArchive() = default;
        ~MemoryArchive() = default;

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
        MemoryArchive &operator<<(const T &v)
        {
            Save(v);
            return *this;
        }

        void Swap(std::vector<uint8_t> &out) { return out.swap(storage); }
        const std::vector<uint8_t> &GetData() const { return storage; }

    private:
        std::vector<uint8_t> storage;
    };

} // namespace sky