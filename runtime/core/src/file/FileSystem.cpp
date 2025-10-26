//
// Created by blues on 2024/4/1.
//

#include <core/file/FileSystem.h>
#include <core/file/FileIO.h>
#include <core/archive/FileArchive.h>
#include <core/util/String.h>
#include <filesystem>

namespace sky {

    FilePath::FilePath() : FilePath("")
    {
    }

    FilePath::FilePath(const char *filePath_) : FilePath(std::string(filePath_))
    {
    }

    FilePath::FilePath(const std::filesystem::path &filePath_) : filePath(filePath_) // NOLINT
    {
    }

    FilePath::FilePath(const std::string &filePath_)
        : filePath(filePath_) // NOLINT
    {
        filePath.make_preferred();
    }

    void FilePath::MakeDirectory() const
    {
        if (!std::filesystem::exists(filePath)) {
            std::filesystem::create_directories(filePath);
        }
    }

    bool FilePath::Exist() const
    {
        return std::filesystem::exists(filePath);
    }

    FilePath FilePath::Parent() const
    {
        return FilePath{filePath.parent_path()};
    }

    FilePath FilePath::FullPath() const
    {
        return FilePath(std::filesystem::absolute(filePath));
    }

    std::string FilePath::FileName() const
    {
        return filePath.filename().string();
    }

    std::string FilePath::FileNameWithoutExt() const
    {
        return filePath.stem().string();
    }

    std::string FilePath::Extension() const
    {
        return filePath.extension().string();
    }

    void FilePath::ReplaceExtension(const std::string &name)
    {
        filePath.replace_extension(name);
    }

    std::fstream FilePath::OpenFStream(std::ios_base::openmode mode) const
    {
        return std::fstream(filePath, mode);
    }

    FilePath& FilePath::operator/=(const FilePath& sub)
    {
        filePath /= sub.filePath;
        return *this;
    }

    FilePath FilePath::operator/(const FilePath& sub) const
    {
        return FilePath(filePath) /= sub;
    }

    std::string FilePath::GetStr() const
    {
        return filePath.string();
    }

    IStreamArchivePtr NativeFile::ReadAsArchive()
    {
        return new IFileArchive(filePath);
    }

    OStreamArchivePtr NativeFile::WriteAsArchive()
    {
        return new OFileArchive(filePath);
    }

    std::string NativeFile::GetPath() const
    {
        return filePath.GetStr();
    }

    bool NativeFile::ReadBin(std::vector<uint8_t> &out)
    {
        return sky::ReadBin(filePath, out);
    }

    BinaryDataPtr NativeFile::ReadBin()
    {
        return sky::ReadBin(filePath);
    }

    bool NativeFile::ReadString(std::string &out)
    {
        return sky::ReadString(filePath, out);
    }

    void NativeFile::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        std::fstream stream = filePath.OpenFStream(std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            return;
        }

        stream.seekg(static_cast<int>(offset), std::ios::beg);
        stream.read(reinterpret_cast<char *>(out), static_cast<int64_t>(size));
    }

    uint64_t NativeFile::AppendData(const char* data, uint64_t size)
    {
        std::fstream file = filePath.OpenFStream(std::ios::out | std::ios::binary | std::ios::app);
        file.write(reinterpret_cast<const char *>(data), size);
        auto offset = file.tellp();
        return static_cast<uint64_t>(offset) - size;
    }

    void RawBufferView::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
    }

    bool RawBufferView::ReadBin(std::vector<uint8_t> &out)
    {
        return true;
    }

    BinaryDataPtr RawBufferView::ReadBin()
    {
        return nullptr;
    }

    bool RawBufferView::ReadString(std::string &out)
    {
        return true;
    }

    IStreamArchivePtr RawBufferView::ReadAsArchive()
    {
        SKY_ASSERT(0) // read only
        return {};
    }

    OStreamArchivePtr RawBufferView::WriteAsArchive()
    {
        SKY_ASSERT(0) // read only
        return {};
    }

    uint64_t RawBufferView::AppendData(const char* data, uint64_t size)
    {
        SKY_ASSERT(0) // read only
        return 0;
    }

    NativeFileSystem::NativeFileSystem(const FilePath &root) : fsRoot(root.FullPath())
    {
        fsRoot.MakeDirectory();
    }

    bool NativeFileSystem::FileExist(const FilePath &path) const
    {
        FilePath res = fsRoot;
        res /= path;
        return res.Exist();
    }

    FilePtr NativeFileSystem::OpenFile(const FilePath &path)
    {
        FilePath res = fsRoot;
        res /= path;

        return res.Exist() ?  new NativeFile(res) : nullptr;
    }

    FilePtr NativeFileSystem::CreateOrOpenFile(const FilePath &path)
    {
        FilePath res = fsRoot;
        res /= path;

        return new NativeFile(res);
    }

    void NativeFileSystem::Copy(const FilePath &from, const FilePath &to) const
    {
        auto fromPath = from.filePath.is_absolute() ? from.filePath : (fsRoot / from).filePath;
        auto toPath = to.filePath.is_absolute() ? to.filePath : (fsRoot / to).filePath;

        std::filesystem::copy(fromPath, toPath, std::filesystem::copy_options::overwrite_existing);
    }

    bool NativeFileSystem::IsSubDir(const std::string &path) const
    {
        return path.find(fsRoot.GetStr()) != std::string::npos;
    }

    FileSystemPtr NativeFileSystem::CreateSubSystem(const std::string &path, bool createDir)
    {
        auto subDir = FilePath(fsRoot.GetStr() + "/" + path);
        if (createDir) {
            subDir.MakeDirectory();
        }
        return new NativeFileSystem(subDir);
    }

    std::vector<FilePath> NativeFileSystem::FilterFiles(const FilePath &path, const std::string &ext)
    {
        std::vector<FilePath> result;

        if (std::filesystem::exists(path.filePath)) {
            for (const auto &entry : std::filesystem::recursive_directory_iterator{path.filePath}) {
                if (entry.is_regular_file() && entry.path().extension() == ext) {
                    result.emplace_back(FilePath( std::filesystem::relative(entry.path(), path.filePath)));
                }
            }
        }

        return result;
    }
} // namespace sky