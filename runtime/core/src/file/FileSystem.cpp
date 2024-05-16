//
// Created by blues on 2024/4/1.
//

#include <core/file/FileSystem.h>
#include <core/file/FileIO.h>
#include <core/archive/FileArchive.h>
#include <filesystem>

namespace sky {

    void NativeFile::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        std::fstream stream(filePath, std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            return;
        }

        stream.seekg(static_cast<int>(offset), std::ios::beg);
        stream.read(reinterpret_cast<char *>(out), static_cast<int64_t>(size));
    }

    bool NativeFileSystem::FileExist(const std::string &path)
    {
        std::filesystem::path rPath(fsRoot.GetStr());
        rPath.append(path);

        return std::filesystem::exists(rPath);
    }

    FilePtr NativeFileSystem::OpenFile(const std::string &path)
    {
        std::filesystem::path rPath(fsRoot.GetStr());
        rPath.append(path);

        return std::make_shared<NativeFile>(rPath.string());
    }

    bool NativeFileSystem::ReadString(const std::string &path, std::string &out)
    {
        std::filesystem::path rPath(fsRoot.GetStr());
        rPath.append(path);

        if (std::filesystem::exists(rPath)) {
            return sky::ReadString(rPath.string(), out);
        }
        return false;
    }

    IArchivePtr NativeFileSystem::ReadAsArchive(const std::string &name)
    {
        std::filesystem::path rPath(fsRoot.GetStr());
        rPath.append(name);

        if (std::filesystem::exists(rPath)) {
            return std::make_shared<IFileArchive>(rPath.string());
        }
        return nullptr;
    }

    OArchivePtr NativeFileSystem::WriteAsArchive(const std::string &name)
    {
        std::filesystem::path rPath(fsRoot.GetStr());
        rPath.append(name);
        return std::make_shared<OFileArchive>(rPath.string());
    }

} // namespace sky