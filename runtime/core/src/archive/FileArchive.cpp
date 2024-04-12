//
// Created by blues on 2024/2/7.
//

#include <core/archive/FileArchive.h>

namespace sky {

    IFileArchive::IFileArchive(const std::string &path)
        : stream(path.c_str(), std::ios::binary | std::ios::in)
        , IStreamArchive(stream)
    {
    }

    OFileArchive::OFileArchive(const std::string &path)
        : stream(path.c_str(), std::ios::binary | std::ios::out)
        , OStreamArchive(stream)
    {
    }

} // namespace sky