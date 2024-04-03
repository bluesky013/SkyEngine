//
// Created by blues on 2024/4/1.
//

#include <core/file/FileSystem.h>
#include <core/file/FileIO.h>
#include <filesystem>

namespace sky {

    NativeFile::NativeFile(const std::string &filePath, std::ios::openmode mode)
        : IFile(filePath)
    {
        fs.open(filePath, mode);
    }

    NativeFile::~NativeFile()
    {
        fs.close();
    }

    bool NativeFile::IsOpen() const
    {
        return fs.is_open();
    }

    FilePtr NativeFileSystem::OpenFile(const std::string &path, std::ios::openmode mode)
    {
        for (auto &root : fsRoot) {
            std::filesystem::path rPath(root.GetStr());
            rPath.append(path);

            if (!std::filesystem::exists(rPath)) {
                continue;
            }

            return std::make_shared<NativeFile>(rPath.string(), mode);
        }
        return {};
    }

    bool NativeFileSystem::ReadString(const std::string &path, std::string &out)
    {
        auto file = OpenFile(path, std::ios::in | std::ios::binary);
        if (!file || !file->IsOpen()) {
            return false;
        }
        return sky::ReadString(file->GetStr(), out);
    }

    void NativeFileSystem::AddPath(const std::string &path)
    {
        fsRoot.emplace_back(path);
    }

} // namespace sky