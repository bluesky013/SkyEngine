//
// Created by blues on 2024/4/1.
//

#pragma once

#include <memory>
#include <string>
#include <fstream>
#include <vector>

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
        explicit IFile(const std::string &filePath_) : filePath(filePath_) {}
        virtual ~IFile() = default;

        virtual bool IsOpen() const { return false; }
        const std::string &GetStr() const { return filePath.GetStr(); }

    protected:
        FilePath filePath;
    };
    using FilePtr = std::shared_ptr<IFile>;

    class NativeFile : public IFile {
    public:
        explicit NativeFile(const std::string &filePath, std::ios::openmode mode);
        ~NativeFile() override;

        bool IsOpen() const override;
    private:
        std::fstream fs;
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

        virtual FilePtr OpenFile(const std::string &path, std::ios::openmode) = 0;
        virtual bool ReadString(const std::string &path, std::string &out) = 0;
    };
    using FileSystemPtr = std::shared_ptr<IFileSystem>;

    class NativeFileSystem : public IFileSystem {
    public:
        NativeFileSystem() = default;
        ~NativeFileSystem() override = default;

        FilePtr OpenFile(const std::string &path, std::ios::openmode) override;
        bool ReadString(const std::string &path, std::string &out) override;

        void AddPath(const std::string &path);
    private:
        std::vector<FilePath> fsRoot;
    };

} // namespace sky