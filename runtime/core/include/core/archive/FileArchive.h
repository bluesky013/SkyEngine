//
// Created by blues on 2024/2/7.
//

#pragma once

#include <core/archive/StreamArchive.h>
#include <fstream>

namespace sky {

    class IFileArchive : public IStreamArchive {
    public:
        explicit IFileArchive(const std::string &path);
        ~IFileArchive() = default;

        bool IsOpen() const { return stream.is_open(); }
    private:
        std::fstream stream;
    };

    class OFileArchive : public OStreamArchive {
    public:
        explicit OFileArchive(const std::string &path);
        ~OFileArchive() = default;

    private:
        std::fstream stream;
    };

} // namespace sky
