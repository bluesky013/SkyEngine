//
// Created by blues on 2024/4/1.
//

#pragma once

#include <memory>
#include <string>
#include <fstream>
#include <vector>
#include <core/archive/IArchive.h>

namespace sky {

    // maybe support for wstring
    class FilePath {
    public:
        explicit FilePath(const std::string &filePath_) : filePath(filePath_) {}
        ~FilePath() = default;

        const std::string &GetStr() const { return filePath; }

    private:
        std::string filePath;
    };

    class IFile {
    public:
        explicit IFile() = default;
        virtual ~IFile() = default;

        virtual void ReadData(uint64_t offset, uint64_t size, uint8_t *out) = 0;
    };
    using FilePtr = std::shared_ptr<IFile>;

    class NativeFile : public IFile {
    public:
        explicit NativeFile(const std::string &path) : filePath(path) {}
        ~NativeFile() override = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;
    private:
        std::string filePath;
    };

    class FileView {
    public:
        FileView(const FilePtr &file_, uint32_t offset_, uint32_t size_) : file(file_), offset(offset_), size(size_) {}
        ~FileView() = default;

    private:
        FilePtr file;
        uint32_t offset;
        uint32_t size;
    };

    class IFileSystem {
    public:
        IFileSystem() = default;
        virtual ~IFileSystem() = default;

        virtual bool FileExist(const std::string &path) = 0;
        virtual IArchivePtr ReadAsArchive(const std::string &name) = 0;
        virtual OArchivePtr WriteAsArchive(const std::string &name) = 0;
        virtual FilePtr OpenFile(const std::string &name) = 0;
        virtual bool ReadString(const std::string &path, std::string &out) = 0;
    };
    using FileSystemPtr = std::shared_ptr<IFileSystem>;

    class NativeFileSystem : public IFileSystem {
    public:
        explicit NativeFileSystem(const std::string &root) : fsRoot(root) {}
        ~NativeFileSystem() override = default;

        bool FileExist(const std::string &path) override;
        FilePtr OpenFile(const std::string &name) override;
        IArchivePtr ReadAsArchive(const std::string &path) override;
        OArchivePtr WriteAsArchive(const std::string &name) override;
        bool ReadString(const std::string &path, std::string &out) override;
    private:
        FilePath fsRoot;
    };
} // namespace sky