//
// Created by blues on 2024/4/1.
//

#include <core/file/FileSystem.h>

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

    NativeFileSystem::NativeFileSystem(const std::string &root)
        : fsRoot(root)
    {
    }

    FileViewPtr NativeFileSystem::CreateFileView(const std::string &path)
    {
        return {};
    }

} // namespace sky