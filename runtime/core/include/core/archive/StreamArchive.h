//
// Created by bluesky on 2023/10/11.
//

#pragma once

#include <iostream>
#include <core/archive/IArchive.h>
#include <core/template/ReferenceObject.h>

namespace sky {

    class StreamArchive;
    using StreamArchivePtr = CounterPtr<StreamArchive>;

    class IStreamArchive;
    using IStreamArchivePtr = CounterPtr<IStreamArchive>;

    class OStreamArchive;
    using OStreamArchivePtr = CounterPtr<OStreamArchive>;

    class StreamArchive : public IInputArchive, public IOutputArchive, public RefObject {
    public:
        explicit StreamArchive(std::iostream &stream_) : stream(stream_) {}
        ~StreamArchive() override = default;

        virtual const char* Data() const { return nullptr; }
        virtual uint32_t Size() const { return 0; }

        bool LoadRaw(char *data, size_t size) override;

        template<class Archive>
        void LoadFromStream(Archive &ar)
        {
            stream << ar.stream.rdbuf();
        }

        bool SaveRaw(const char *data, size_t size) override;

        std::iostream &GetStream() const { return stream; }

        int Peek() override;
        int Get() override;
        size_t Tell() const override;
        void Put(char ch) override;
    protected:
        std::iostream &stream;
    };

    class IStreamArchive : public IInputArchive, public RefObject {
    public:
        explicit IStreamArchive(std::istream &s) : stream(s) {}
        ~IStreamArchive() override = default;

        bool LoadRaw(char *data, size_t size) override;

        std::istream &GetStream() const { return stream; }

        int Peek() override;
        int Get() override;
        size_t Tell() const override;
    private:
        friend class StreamArchive;

        std::istream &stream;
    };

    class OStreamArchive : public IOutputArchive, public RefObject {
    public:
        explicit OStreamArchive(std::ostream &s) : stream(s) {}
        ~OStreamArchive() override = default;

        bool SaveRaw(const char *data, size_t size) override;

        std::ostream &GetStream() const { return stream; }

        void Put(char ch) override;
        void Flush() override;
    private:
        friend class StreamArchive;

        std::ostream &stream;
    };

} // namespace sky