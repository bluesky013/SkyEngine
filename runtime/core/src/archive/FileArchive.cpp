//
// Created by blues on 2024/2/7.
//

#include <core/archive/FileArchive.h>

namespace sky {

    IFileArchive::IFileArchive(const std::string &path, std::ios::openmode mode)
        : stream(path.c_str(), mode | std::ios::in)
        , IStreamArchive(stream)
    {
    }

    OFileArchive::OFileArchive(const std::string &path, std::ios::openmode mode)
        : stream(path.c_str(), mode | std::ios::out)
        , OStreamArchive(stream)
    {
    }

} // namespace sky
