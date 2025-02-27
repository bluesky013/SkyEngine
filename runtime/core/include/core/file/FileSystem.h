//
// Created by blues on 2024/4/1.
//

#pragma once

#include <memory>
#include <string>
#include <fstream>
#include <vector>
#include <filesystem>
#include <core/archive/StreamArchive.h>
#include <core/template/ReferenceObject.h>

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
        FilePath FullPath() const;
        std::string FileName() const;
        std::string FileNameWithoutExt() const;
        std::string Extension() const;
        void ReplaceExtension(const std::string &name);

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

        virtual IStreamArchivePtr ReadAsArchive() = 0;
        virtual OStreamArchivePtr WriteAsArchive() = 0;

        virtual bool ReadBin(std::vector<uint8_t> &out) = 0;
        virtual bool ReadString(std::string &out) = 0;

        virtual uint64_t AppendData(const char* data, uint64_t size) = 0;
        virtual std::string GetPath() const = 0;
    };

    class NativeFile : public IFile {
    public:
        explicit NativeFile(const FilePath &path) : filePath(path) {}
        ~NativeFile() override = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;
        bool ReadBin(std::vector<uint8_t> &out) override;
        bool ReadString(std::string &out) override;

        uint64_t AppendData(const char* data, uint64_t size) override;

        IStreamArchivePtr ReadAsArchive() override;
        OStreamArchivePtr WriteAsArchive() override;

        std::string GetPath() const override;

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

        uint64_t AppendData(const char* data, uint64_t size) override;

        std::string GetPath() const override { return ""; }

        IStreamArchivePtr ReadAsArchive() override;
        OStreamArchivePtr WriteAsArchive() override;
    };

    class IFileSystem : public RefObject {
    public:
        IFileSystem() = default;
        ~IFileSystem() override = default;

        virtual void Copy(const FilePath &from, const FilePath &to) const {}
        virtual bool FileExist(const FilePath &path) const = 0;
        virtual FilePtr OpenFile(const FilePath &name) = 0;
        virtual FilePtr CreateOrOpenFile(const FilePath &name) = 0;
        virtual bool IsReadOnly() const { return true; }
        virtual FileSystemPtr CreateSubSystem(const std::string &path, bool createDir) { return nullptr; }
        virtual const FilePath &GetPath() const { static FilePath path; return path; }
    };

    class NativeFileSystem : public IFileSystem {
    public:
        explicit NativeFileSystem(const FilePath &root);
        ~NativeFileSystem() override = default;

        bool FileExist(const FilePath &path) const override;
        FilePtr OpenFile(const FilePath &path) override;
        FilePtr CreateOrOpenFile(const FilePath &path) override;

        bool IsReadOnly() const override { return false; }
        const FilePath &GetPath() const override { return fsRoot; }

        void Copy(const FilePath &from, const FilePath &to) const override;
        bool IsSubDir(const std::string &path) const;
        FileSystemPtr CreateSubSystem(const std::string &path, bool createDir) override;

        static std::vector<FilePath> FilterFiles(const FilePath &path, const std::string &ext);
    private:
        FilePath fsRoot;
    };

    class PackageFileSystem : public IFileSystem {
    public:
        explicit PackageFileSystem(const std::string &pakPath) {}
        ~PackageFileSystem() override = default;
    };
} // namespace sky