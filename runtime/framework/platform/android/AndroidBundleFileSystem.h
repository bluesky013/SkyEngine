//
// Created by blues on 2024/5/16.
//

#pragma once

#include <core/file/FileSystem.h>

namespace sky {

    class AndroidAssetFile : public IFile {
    public:
        explicit AndroidAssetFile(const FilePath &name) : path(name.GetStr()) {}
        ~AndroidAssetFile() override = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;

        IStreamArchivePtr ReadAsArchive() override;
        OStreamArchivePtr WriteAsArchive() override;

        bool ReadBin(std::vector<uint8_t> &out) override;
        bool ReadString(std::string &out) override;

        uint64_t AppendData(const char* data, uint64_t size) override { return 0; }
        std::string GetPath() const override { return path; }

//        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;
    private:
        std::string path;
    };

    class AndroidBundleFileSystem : public IFileSystem {
    public:
        explicit AndroidBundleFileSystem(const std::string& basePath);
        ~AndroidBundleFileSystem() override = default;

        bool FileExist(const FilePath &path) const override;
//        IArchivePtr ReadAsArchive(const std::string &path) override;
//        OArchivePtr WriteAsArchive(const std::string &path) override;
        FilePtr OpenFile(const FilePath &name) override;
        FilePtr CreateOrOpenFile(const FilePath &name) override;
        FileSystemPtr CreateSubSystem(const std::string &path, bool createDir);

    private:
        std::string basePath;
    };

} // namespace sky
