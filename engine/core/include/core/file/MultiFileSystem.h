//
// Created on 2026/09/12.
//

#pragma once

#include <core/file/FileSystem.h>

#include <vector>

namespace sky {

    // Composes an ordered list of file systems. Earlier mounts take priority:
    // FileExist/OpenFile search front-to-back and return the first match,
    // giving deterministic search-path semantics across native dirs, packages,
    // and in-memory sources.
    class MultiFileSystem : public IFileSystem {
    public:
        MultiFileSystem() = default;
        ~MultiFileSystem() override = default;

        void AddFileSystem(FileSystemPtr fs);

        bool FileExist(const FilePath &path) const override;
        FilePtr OpenFile(const FilePath &path) override;
        FilePtr CreateOrOpenFile(const FilePath &path) override;
        bool IsReadOnly() const override;

    private:
        std::vector<FileSystemPtr> mFileSystems;
    };

} // namespace sky
