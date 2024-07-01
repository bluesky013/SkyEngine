//
// Created by blues on 2024/2/7.
//

#pragma once

#include <core/archive/StreamArchive.h>
#include <core/archive/MemoryArchive.h>
#include <core/file/FileSystem.h>
#include <fstream>

namespace sky {

    class IFileArchive : public IStreamArchive {
    public:
        explicit IFileArchive(const FilePath &path, std::ios::openmode mode = std::ios::binary);
        ~IFileArchive() override = default;

        using IInputArchive::LoadRaw;

        bool IsOpen() const { return stream.is_open(); }
    private:
        std::fstream stream;
    };

    class OFileArchive : public OStreamArchive {
    public:
        explicit OFileArchive(const FilePath &path, std::ios::openmode mode = std::ios::binary);
        ~OFileArchive() override = default;

        using IOutputArchive::SaveRaw;
    private:
        std::fstream stream;
    };

} // namespace sky
