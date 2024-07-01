//
// Created by bluesky on 2023/10/11.
//

#pragma once

#include <iostream>
#include <core/archive/IArchive.h>

namespace sky {

    class IStreamArchive : public IInputArchive {
    public:
        explicit IStreamArchive(std::istream &s) : stream(s) {}
        ~IStreamArchive() override = default;

        using IInputArchive::LoadRaw;

    private:
        bool LoadRaw(char *data, size_t size) override;

        int Peek() const override;
        int Get() override;
        size_t Tell() const override;

        std::istream &stream;
    };

    class OStreamArchive : public IOutputArchive {
    public:
        explicit OStreamArchive(std::ostream &s) : stream(s) {}
        ~OStreamArchive() override = default;

        using IOutputArchive::SaveRaw;

        void Put(char ch) override;
        void Flush() override;
    private:
        bool SaveRaw(const char *data, size_t size) override;
        std::ostream &stream;
    };

} // namespace sky