//
// Created by Zach Lee on 2023/2/19.
//

#pragma once

#include <iostream>
#include <core/util/Uuid.h>
#include <core/platform/Platform.h>
#include <core/archive/ArchiveConcept.h>

namespace sky {

    class BinaryInputArchive {
    public:
        explicit BinaryInputArchive(std::istream &s) : stream(s)
        {
        }

        ~BinaryInputArchive() = default;

        void LoadValue(char* data, uint32_t size)
        {
            auto const writtenSize = stream.rdbuf()->sgetn(data, size);
            SKY_ASSERT(writtenSize == size);
        }

        template <typename T, typename = std::enable_if<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        void LoadValue(T &v)
        {
            LoadValue(reinterpret_cast<char*>(std::addressof(v)), sizeof(T));
        }

        void LoadValue(std::string &v)
        {
            uint32_t length = 0;
            LoadValue(length);
            v.resize(length, 0);
            LoadValue(v.data(), length);
        }

        void LoadObject(void *ptr, const Uuid &id);
    protected:
        std::istream &stream;
    };

    class BinaryOutputArchive {
    public:
        explicit BinaryOutputArchive(std::ostream &s) : stream(s)
        {
        }
        ~BinaryOutputArchive() = default;

        void SaveValue(const char* data, uint32_t size)
        {
            auto const writtenSize = stream.rdbuf()->sputn(data, size);
            SKY_ASSERT(writtenSize == size);
        }

        template <typename T, typename = std::enable_if<std::is_arithmetic_v<T> || std::is_enum_v<T>>>
        void SaveValue(const T &v)
        {
            SaveValue(reinterpret_cast<const char*>(std::addressof(v)), sizeof(T));
        }

        void SaveValue(const std::string &v)
        {
            SaveValue(static_cast<uint32_t>(v.length()));
            SaveValue(v.data(), static_cast<uint32_t>(v.length()));
        }

        void SaveObject(const void* data, const Uuid &id);
    protected:
        std::ostream &stream;
    };
}
