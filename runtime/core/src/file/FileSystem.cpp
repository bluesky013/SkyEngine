//
// Created by blues on 2024/4/1.
//

#include <core/file/FileSystem.h>
#include <core/file/FileIO.h>
#include <core/archive/FileArchive.h>
#include <core/util/String.h>
#include <filesystem>

namespace sky {

    FilePath::FilePath(const char* filePath_) : filePath(filePath_)
    {
    }

    FilePath::FilePath(const std::string &filePath_) : filePath(filePath_)
    {
    }

    std::filesystem::path FilePath::ConvertStdFSPath() const
    {
#ifdef SKY_FS_USE_WCHAR
        return std::filesystem::path{Utf8ToUtf16(filePath)};
#else
        return std::filesystem::path{filePath};
#endif
    }

    void FilePath::MakeDirectory() const
    {
        auto path = ConvertStdFSPath();
        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directories(path);
        }
    }

    FilePath& FilePath::operator/=(const FilePath& sub)
    {
        filePath += "/" + sub.filePath;
        return *this;
    }

    const std::string &FilePath::GetStr() const
    {
        return filePath;
    }

    IArchivePtr NativeFile::ReadAsArchive()
    {
        return new IFileArchive(filePath);
    }

    OArchivePtr NativeFile::WriteAsArchive()
    {
        return new OFileArchive(filePath);
    }

    bool NativeFile::ReadBin(std::vector<uint8_t> &out)
    {
        return sky::ReadBin(filePath, out);
    }

    bool NativeFile::ReadString(std::string &out)
    {
        return sky::ReadString(filePath, out);
    }

    void NativeFile::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        std::fstream stream(filePath.ConvertStdFSPath(), std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            return;
        }

        stream.seekg(static_cast<int>(offset), std::ios::beg);
        stream.read(reinterpret_cast<char *>(out), static_cast<int64_t>(size));
    }

    void RawBufferView::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
    }

    bool RawBufferView::ReadBin(std::vector<uint8_t> &out)
    {
        return true;
    }

    bool RawBufferView::ReadString(std::string &out)
    {
        return true;
    }

    IArchivePtr RawBufferView::ReadAsArchive()
    {
        return {};
    }

    OArchivePtr RawBufferView::WriteAsArchive()
    {
        return {};
    }

    bool NativeFileSystem::FileExist(const FilePath &path) const
    {
        FilePath res = fsRoot;
        res /= path;
        return std::filesystem::exists(res.ConvertStdFSPath());
    }

    FilePtr NativeFileSystem::OpenFile(const FilePath &path)
    {
        FilePath res = fsRoot;
        res /= path;
        return std::filesystem::exists(res.ConvertStdFSPath()) ? new NativeFile(res) : nullptr;
    }

    FilePtr NativeFileSystem::CreateOrOpenFile(const FilePath &path)
    {
        FilePath res = fsRoot;
        res /= path;

        return new NativeFile(res);
    }

    bool NativeFileSystem::IsSubDir(const std::string &path) const
    {
        return path.find(fsRoot.GetStr()) != std::string::npos;
    }

    NativeFileSystemPtr NativeFileSystem::CreateSubSystem(const std::string &path, bool createDir)
    {
        auto subDir = FilePath(fsRoot.GetStr() + "/" + path);
        if (createDir) {
            subDir.MakeDirectory();
        }
        return new NativeFileSystem(subDir);
    }
} // namespace sky