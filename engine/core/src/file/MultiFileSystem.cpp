//
// Created on 2026/09/12.
//

#include <core/file/MultiFileSystem.h>

#include <utility>

namespace sky {

    void MultiFileSystem::AddFileSystem(FileSystemPtr fs)
    {
        mFileSystems.emplace_back(std::move(fs));
    }

    bool MultiFileSystem::FileExist(const FilePath &path) const
    {
        for (const auto &fs : mFileSystems) {
            if (fs->FileExist(path)) {
                return true;
            }
        }
        return false;
    }

    FilePtr MultiFileSystem::OpenFile(const FilePath &path)
    {
        for (const auto &fs : mFileSystems) {
            if (!fs->FileExist(path)) {
                continue;
            }
            auto file = fs->OpenFile(path);
            if (file != nullptr) {
                return file;
            }
        }
        return nullptr;
    }

    FilePtr MultiFileSystem::CreateOrOpenFile(const FilePath &path)
    {
        for (const auto &fs : mFileSystems) {
            if (!fs->IsReadOnly()) {
                return fs->CreateOrOpenFile(path);
            }
        }
        return nullptr;
    }

    bool MultiFileSystem::IsReadOnly() const
    {
        for (const auto &fs : mFileSystems) {
            if (!fs->IsReadOnly()) {
                return false;
            }
        }
        return true;
    }

} // namespace sky
