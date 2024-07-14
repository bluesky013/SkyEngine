//
// Created by blues on 2024/2/7.
//

#include <core/archive/FileArchive.h>

namespace sky {

    IFileArchive::IFileArchive(const FilePath &path, std::ios::openmode mode)
        : stream(path.OpenFStream(mode | std::ios::in))
        , IStreamArchive(stream)
    {
        SKY_ASSERT(stream.is_open());
    }

    OFileArchive::OFileArchive(const FilePath &path, std::ios::openmode mode)
        : stream(path.OpenFStream(mode | std::ios::out))
        , OStreamArchive(stream)
    {
        SKY_ASSERT(stream.is_open());
    }

} // namespace sky
