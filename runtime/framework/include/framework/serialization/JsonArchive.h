//
// Created by Zach Lee on 2023/1/18.
//

#pragma once

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <framework/serialization/Any.h>
#include <core/platform/Platform.h>
#include <core/archive/StreamArchive.h>
#include <iostream>
#include <vector>

namespace sky {

    namespace impl {

        class InputStreamWrapper {
        public:
            using Ch = std::istream::char_type;
            explicit InputStreamWrapper(IInputArchive &s) : stream(s)
            {
            }

            InputStreamWrapper(const InputStreamWrapper &)            = delete;
            InputStreamWrapper &operator=(const InputStreamWrapper &) = delete;

            Ch Peek() const
            {
                int c = stream.Peek();
                return c == std::char_traits<char>::eof() ? '\0' : static_cast<Ch>(c);
            }

            Ch Take()
            {
                int c = stream.Get();
                return c == std::char_traits<char>::eof() ? '\0' : static_cast<Ch>(c);
            }

            size_t Tell() const
            {
                return static_cast<size_t>(stream.Tell());
            }

            Ch *PutBegin()      { SKY_ASSERT(false); return nullptr; } // NOLINT
            void Put(Ch)        { SKY_ASSERT(false); } // NOLINT
            void Flush()        { SKY_ASSERT(false); } // NOLINT
            size_t PutEnd(Ch *) { SKY_ASSERT(false); return 0; } // NOLINT

        private:
            IInputArchive &stream;
        };

        class OutputStreamWrapper {
        public:
            using Ch = std::istream::char_type;
            explicit OutputStreamWrapper(IOutputArchive &s) : stream(s)
            {
            }

            OutputStreamWrapper(const OutputStreamWrapper &) = delete;
            OutputStreamWrapper &operator=(const OutputStreamWrapper &) = delete;

            void Put(Ch c)
            {
                stream.Put(c);
            }

            void Flush()
            {
                stream.Flush();
            }

        private:
            IOutputArchive &stream;
        };
    }

    class JsonInputArchive {
    public:
        using Value = rapidjson::GenericValue<rapidjson::UTF8<>>;
        using Stream = impl::InputStreamWrapper;

        explicit JsonInputArchive(IInputArchive &archive) : stream(archive)
        {
            document.ParseStream(stream);
            stack.emplace_back(&document);
        }

        ~JsonInputArchive() = default;

        bool Start(const std::string &name)
        {
            const auto &parent = *stack.back();
            auto iter = parent.FindMember(name.c_str());
            if (iter == parent.MemberEnd()) {
                return false;
            }
            if (iter->value.IsArray()) {
                stack.emplace_back(iter->value.GetArray().begin());
            } else {
                stack.emplace_back(&iter->value);
            }
            return true;
        }

        uint32_t StartArray(const std::string &name)
        {
            const auto &parent = *stack.back();
            auto iter = parent.FindMember(name.c_str());
            if (iter != parent.MemberEnd() && iter->value.IsArray()) {
                auto array = iter->value.GetArray();
                stack.emplace_back(array.begin());
                return array.Size();
            }
            return 0;
        }

        template<class Func>
        void ForEachMember(Func &&func)
        {
            const auto &parent = *stack.back();
            for (auto iter = parent.MemberBegin(); iter != parent.MemberEnd(); ++iter)
            {
                const auto &member = *iter;

                if constexpr (std::is_invocable_v<Func, const std::string>) {
                    func(member.name.GetString());
                } else {
                    func(member.name.GetString(), member.value);
                }
            }
        }

        void End()
        {
            if (!stack.empty()) {
                stack.pop_back();
            }
        }

        bool LoadBool();
        int32_t LoadInt();
        uint32_t LoadUint();
        int64_t LoadInt64();
        uint64_t LoadUint64();
        double LoadDouble();
        std::string LoadString();

        template <typename T>
        void LoadArrayElement(T &value)
        {
            LoadValueById(&value, TypeInfo<T>::RegisteredId());
            stack.back()++;
        }

        void NextArrayElement()
        {
            stack.back()++;
        }

        Any LoadValueById(const Uuid & typeId);
        void LoadValueById(void *ptr, const Uuid &typeId);

        template <typename T>
        void LoadValueObject(T &value)
        {
            LoadValueById(&value, TypeInfo<T>::RegisteredId());
        }

        template <typename T>
        void LoadKeyValue(const std::string &name, T &value)
        {
            Start(name);
            if constexpr (std::is_enum_v<T>) {
                using UType = std::underlying_type_t<T>;
                UType v = 0;
                LoadValueObject(v);
                value = static_cast<T>(v);
            } else {
                LoadValueObject(value);
            }

            End();
        }

    private:
        Stream stream;
        rapidjson::Document document;
        std::vector<const Value*> stack;
    };

    class JsonOutputArchive {
    public:
        using Stream = impl::OutputStreamWrapper;
        using Writter = rapidjson::PrettyWriter<Stream>;

        explicit JsonOutputArchive(IOutputArchive &s) : stream(s), writer(stream) {}
        ~JsonOutputArchive() = default;

        void StartObject() { writer.StartObject(); }
        void EndObject()   { writer.EndObject(); };

        void StartArray()  { writer.StartArray(); }
        void EndArray()    { writer.EndArray(); }

        template <typename T>
        void SaveEnum(const T &v)
        {
            static_assert(std::is_enum_v<T>);
            SaveValue(static_cast<std::underlying_type_t<T>>(v));
        }

        void SaveValue(bool v)       { writer.Bool(v); }
        void SaveValue(uint32_t v)   { writer.Uint(v); }
        void SaveValue(int32_t v)    { writer.Int(v); }
        void SaveValue(int64_t i64)  { writer.Int64(i64); }
        void SaveValue(uint64_t u64) { writer.Uint64(u64); }
        void SaveValue(double v)     { writer.Double(v); }

        void SaveValue(std::string const & s) { writer.String(s.c_str(), static_cast<rapidjson::SizeType>(s.size())); }
        void SaveValue(char const * s)        { writer.String(s); }
        void SaveValue(std::nullptr_t)        { writer.Null(); }

        void SaveValueObject(const Any &any);
        void SaveValueObject(const void *ptr, const Uuid &id);
        template <typename T, typename = std::enable_if<!std::is_same_v<T, Any>, void>>
        void SaveValueObject(const T& value)
        {
            SaveValueObject(&value, TypeInfo<T>::RegisteredId());
        }

        template <typename T>
        void SaveValueObject(const std::string &key, const T&value)
        {
            Key(key.c_str());
            if constexpr (std::is_enum_v<T>) {
                SaveValueObject(static_cast<std::underlying_type_t<T>>(value));
            } else {
                SaveValueObject(value);
            }

        }

        void Key(const char* key) { writer.Key(key); }

    protected:
        Stream stream;
        Writter writer;
    };

} // namespace sky
