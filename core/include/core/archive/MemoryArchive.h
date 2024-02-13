//
// Created by blues on 2024/1/14.
//

#pragma once

#include <vector>

namespace sky {

    class MemoryArchive {
    public:
        MemoryArchive() = default;
        ~MemoryArchive() = default;

        bool Save(const uint8_t *data, size_t size);

        template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        bool Save(const T &v)
        {
            return Save(reinterpret_cast<const uint8_t *>(&v), sizeof(T));
        }

        template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        MemoryArchive &operator<<(const T &v)
        {
            Save(v);
            return *this;
        }

        void Swap(std::vector<uint8_t> &out) { return out.swap(storage); }

    private:
        std::vector<uint8_t> storage;
    };

} // namespace sky