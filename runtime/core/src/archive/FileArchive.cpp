//
// Created by blues on 2024/2/7.
//

#include <core/archive/FileArchive.h>

namespace sky {

    IFileArchive::IFileArchive(const FilePath &path, std::ios::openmode mode)
        : stream(path.ConvertStdFSPath(), mode | std::ios::in)
        , IStreamArchive(stream)
    {
    }

    OFileArchive::OFileArchive(const FilePath &path, std::ios::openmode mode)
        : stream(path.ConvertStdFSPath(), mode | std::ios::out)
        , OStreamArchive(stream)
    {
    }

} // namespace sky
