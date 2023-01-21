//
// Created by Zach Lee on 2023/1/18.
//

#pragma once

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <core/type/Any.h>
#include <core/platform/Platform.h>
#include <iostream>
#include <vector>

namespace sky {

    namespace impl {

        template <typename StreamType>
        class InputStreamWrapper {
        public:
            using Ch = typename StreamType::char_type;
            InputStreamWrapper(StreamType &s) : stream(s)
            {
            }

            InputStreamWrapper(const InputStreamWrapper &)            = delete;
            InputStreamWrapper &operator=(const InputStreamWrapper &) = delete;

            Ch Peek() const
            {
                int c = stream.peek();
                return c == std::char_traits<char>::eof() ? '\0' : static_cast<Ch>(c);
            }

            Ch Take()
            {
                int c = stream.get();
                return c == std::char_traits<char>::eof() ? '\0' : static_cast<Ch>(c);
            }

            size_t Tell() const
            {
                return static_cast<size_t>(stream.tellg());
            }

            Ch *PutBegin()      { SKY_ASSERT(false); return 0; }
            void Put(Ch)        { SKY_ASSERT(false); }
            void Flush()        { SKY_ASSERT(false); }
            size_t PutEnd(Ch *) { SKY_ASSERT(false); return 0; }

        private:
            StreamType &stream;
        };


        template <typename StreamType>
        class OutputStreamWrapper {
        public:
            using Ch = typename StreamType::char_type;
            OutputStreamWrapper(StreamType &s) : stream(s)
            {
            }

            OutputStreamWrapper(const OutputStreamWrapper &) = delete;
            OutputStreamWrapper &operator=(const OutputStreamWrapper &) = delete;

            void Put(Ch c)
            {
                stream.put(c);
            }

            void Flush()
            {
                stream.flush();
            }

        private:
            StreamType &stream;
        };
    }

    class JsonInputArchive {
    public:
        using Value = rapidjson::GenericValue<rapidjson::UTF8<>>;
        using MemberIterator = Value::ConstMemberIterator;
        using ValueIterator = Value::ConstValueIterator ;
        using ValueArray = Value::ConstArray;
        using Stream = impl::InputStreamWrapper<std::istream>;

        JsonInputArchive(std::istream &s) : stream(s)
        {
            document.ParseStream(stream);
            stack.emplace_back(&document);
        }

        ~JsonInputArchive() = default;

        bool Start(const std::string &name)
        {
            auto &parent = *stack.back();
            auto iter = parent.FindMember(name.c_str());
            if (iter == parent.MemberEnd()) {
                return false;
            }
            stack.emplace_back(&iter->value);
            return true;
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

        uint32_t LoadArray()
        {
            SKY_ASSERT(!stack.empty());
            auto value = stack.back();
            SKY_ASSERT(value != nullptr && value->IsArray());
            auto array = value->GetArray();
            begin = array.begin();
            end = array.end();
            return static_cast<uint32_t>(array.Size());
        }

        template <typename T>
        void LoadArrayElement(T &value)
        {
            SKY_ASSERT(begin != end);
            stack.emplace_back(begin);
            value = *(LoadValueById(TypeInfo<T>::Hash()).template GetAs<T>());
            stack.pop_back();
            ++begin;
        }

        Any LoadValueById(uint32_t typeId);
        void LoadValueObject(void *ptr, uint32_t typeId);

        template <typename T>
        void LoadValueObject(T &value)
        {
            LoadValueObject(&value, TypeInfo<T>::Hash());
        }

    private:
        Stream stream;
        rapidjson::Document document;
        std::vector<const Value*> stack;
        ValueIterator begin = nullptr;
        ValueIterator end = nullptr;
    };

    class JsonOutputArchive {
    public:
        using Stream = impl::OutputStreamWrapper<std::ostream>;
        using Writter = rapidjson::PrettyWriter<Stream>;

        JsonOutputArchive(std::ostream &s) : stream(s), writer(stream) {}
        ~JsonOutputArchive() = default;

        void SaveValueObject(const Any &any);
        void SaveValueObject(const void *ptr, uint32_t id);
        template <typename T, typename = std::enable_if<!std::is_same_v<T, Any>, void>>
        void SaveValueObject(const T& value)
        {
            SaveValueObject(&value, TypeInfo<T>::Hash());
        }

        void StartObject() { writer.StartObject(); }
        void EndObject()   { writer.EndObject(); };

        void StartArray()  { writer.StartArray(); }
        void EndArray()    { writer.EndArray(); }

        void SaveValue(bool v)       { writer.Bool(v); }
        void SaveValue(uint32_t v)   { writer.Uint(v); }
        void SaveValue(int32_t v)    { writer.Int(v); }
        void SaveValue(int64_t i64)  { writer.Int64(i64); }
        void SaveValue(uint64_t u64) { writer.Uint64(u64); }
        void SaveValue(double v)     { writer.Double(v); }

        void SaveValue(std::string const & s) { writer.String(s.c_str(), static_cast<rapidjson::SizeType>(s.size())); }
        void SaveValue(char const * s)        { writer.String(s); }
        void SaveValue(std::nullptr_t)        { writer.Null(); }

        void Key(const char* key) { writer.Key(key); }

    protected:
        Stream stream;
        Writter writer;
    };

} // namespace sky
