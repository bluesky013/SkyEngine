//
// Created by Zach Lee on 2023/1/18.
//

#pragma once

#include <framework/serialization/Archive.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/prettywriter.h>
#include <core/type/Any.h>

namespace sky {

    namespace impl {
        template <typename StreamType>
        class StreamWrapper {
        public:
            typedef typename StreamType::char_type Ch;
            StreamWrapper(StreamType &stream) : stream_(stream)
            {
            }

            StreamWrapper(const StreamWrapper &) = delete;
            StreamWrapper &operator=(const StreamWrapper &) = delete;

            void Put(Ch c)
            {
                stream_.put(c);
            }

            void Flush()
            {
                stream_.flush();
            }

        private:
            StreamType &stream_;
        };
    }

    class JsonInputArchive : public InputArchive {
    public:
        JsonInputArchive(std::istream &s) : stream(s) {}
        ~JsonInputArchive() = default;

    private:
        std::istream &stream;
    };

    class JsonOutputArchive : public OutputArchive {
    public:
        using Stream = impl::StreamWrapper<std::ostream>;
        using Writter = rapidjson::PrettyWriter<Stream>;

        JsonOutputArchive(std::ostream &s) : stream(s), writer(stream) {}
        ~JsonOutputArchive() = default;

        void SaveValue(const Any &any);

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