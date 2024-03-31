//
// Created by blues on 2024/4/1.
//

#pragma once

#include <memory>
#include <string>
#include <fstream>

namespace sky {

    // maybe support for wstring
    class FilePath {
    public:
        explicit FilePath(const std::string &filePath_) : filePath(filePath_) {}
        ~FilePath() = default;

    private:
        std::string filePath;
    };

    class IFile {
    public:
        explicit IFile(const std::string &filePath_) : filePath(filePath_) {}
        virtual ~IFile() = default;

        virtual bool IsOpen() const { return false; }

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
    using FileViewPtr = std::shared_ptr<FileView>;

    class IFileSystem {
    public:
        IFileSystem() = default;
        virtual ~IFileSystem() = default;

        virtual FileViewPtr CreateFileView(const std::string &path) = 0;
    };

    class NativeFileSystem : public IFileSystem {
    public:
        explicit NativeFileSystem(const std::string &root);
        ~NativeFileSystem() override = default;

        FileViewPtr CreateFileView(const std::string &path) override;
    private:
        FilePath fsRoot;
    };

} // namespace sky