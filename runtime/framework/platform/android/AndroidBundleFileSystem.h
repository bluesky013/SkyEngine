//
// Created by blues on 2024/5/16.
//

#pragma once

#include <core/file/FileSystem.h>

namespace sky {

    class AndroidAssetFile : public IFile {
    public:
        explicit AndroidAssetFile(const std::string &name) : path(name) {}
        ~AndroidAssetFile() = default;

        void ReadData(uint64_t offset, uint64_t size, uint8_t *out) override;
    private:
        std::string path;
    };

    class AndroidBundleFileSystem : public IFileSystem {
    public:
        AndroidBundleFileSystem() = default;
        ~AndroidBundleFileSystem() override = default;

        bool FileExist(const std::string &path) override;
        IArchivePtr ReadAsArchive(const std::string &path) override;
        OArchivePtr WriteAsArchive(const std::string &path) override;
        FilePtr OpenFile(const std::string &name) override;
        bool ReadString(const std::string &path, std::string &out) override;
    };

} // namespace sky
