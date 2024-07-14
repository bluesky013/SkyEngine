//
// Created by blues on 2024/4/1.
//

#pragma once

#include <memory>
#include <string>
#include <fstream>
#include <vector>
#include <filesystem>
#include <core/archive/IArchive.h>
#include <core/template/ReferenceObject.h>

#if WIN32
#define SKY_FS_USE_WCHAR
#endif

namespace sky {

    class IFile;
    class IFileSystem;

    using FilePtr = CounterPtr<IFile>;
    using FileSystemPtr = CounterPtr<IFileSystem>;

    class NativeFileSystem;
    using NativeFileSystemPtr = CounterPtr<NativeFileSystem>;


    // maybe support for wstring
    class FilePath {
    public:
        FilePath();
        FilePath(const char *filePath_); // NOLINT
        FilePath(const std::string &filePath_); // NOLINT
        FilePath(const std::filesystem::path &filePath_); // NOLINT
        ~FilePath() = default;

        FilePath& operator/=(const FilePath& sub);
        FilePath operator/(const FilePath& sub) const;

        std::string GetStr() const;
        void MakeDirectory() const;
        bool Exist() const;
        FilePath Parent() const;
        std::string FileName() const;
        std::string FileNameWithoutExt() const;
        std::string Extension() const;

        std::fstream OpenFStream(std::ios_base::openmode) const;

        bool operator==(const FilePath &rhs) const { return filePath == rhs.filePath; }
    private:
        friend class NativeFileSystem;

        std::filesystem::path filePath;
    };

    class IFile : public RefObject {
    public:
        explicit IFile() = default;
        ~IFile() override = default;

        virtual void ReadData(uint64_t offset, uint64_t size, uint8_t *out) = 0;

        virtual IArchivePtr ReadAsArchive() = 0;
        virtual OArchivePtr WriteAsArchive() = 0;

        virtual bool ReadBin(std::vector<uint8_t> &out) = 0;
        virtual bool ReadString(std::string &out) = 0;

//        virtual std::istream ReadAsStream(const FilePath &name) = 0;
//        virtual std::ostream WriteAsStream(const FilePath &name) = 0;

    };

    class NativeFile : public IFile {
    public:
        explicit NativeFile(const FilePath &path) : filePath(path) {}
        ~NativeFile() override = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;
        bool ReadBin(std::vector<uint8_t> &out) override;
        bool ReadString(std::string &out) override;

        IArchivePtr ReadAsArchive() override;
        OArchivePtr WriteAsArchive() override;

//        std::istream ReadAsStream(const FilePath &name) override;
//        std::ostream WriteAsStream(const FilePath &name) override;

    private:
        FilePath filePath;
    };

    class RawBufferView : public IFile {
    public:
        explicit RawBufferView() = default;
        ~RawBufferView() override = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;
        bool ReadBin(std::vector<uint8_t> &out) override;
        bool ReadString(std::string &out) override;

        IArchivePtr ReadAsArchive() override;
        OArchivePtr WriteAsArchive() override;
    };

    class IFileSystem : public RefObject {
    public:
        IFileSystem() = default;
        ~IFileSystem() override = default;

        virtual bool FileExist(const FilePath &path) const = 0;
        virtual FilePtr OpenFile(const FilePath &name) = 0;
        virtual FilePtr CreateOrOpenFile(const FilePath &name) = 0;
    };

    class NativeFileSystem : public IFileSystem {
    public:
        explicit NativeFileSystem(const FilePath &root) : fsRoot(root) {}
        ~NativeFileSystem() override = default;

        bool FileExist(const FilePath &path) const override;
        FilePtr OpenFile(const FilePath &path) override;
        FilePtr CreateOrOpenFile(const FilePath &path) override;
        const FilePath &GetPath() const { return fsRoot; }

        void Copy(const FilePath &from, const FilePath &to) const;
        bool IsSubDir(const std::string &path) const;
        NativeFileSystemPtr CreateSubSystem(const std::string &path, bool createDir);
    private:
        FilePath fsRoot;
    };

    class PackageFileSystem : public IFileSystem {
    public:
        explicit PackageFileSystem(const std::string &pakPath) {}
        ~PackageFileSystem() override = default;
    };
} // namespace sky