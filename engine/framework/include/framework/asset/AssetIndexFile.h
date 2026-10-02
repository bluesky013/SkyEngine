//
// Created by blues on 2026/10/2.
//

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <core/file/FileSystem.h>
#include <core/util/Uuid.h>

namespace sky {

    struct IndexFileEntry {
        std::string key;
        Uuid id;
        std::string extra; // raw JSON (e.g. a cook block), empty when absent
    };

    // Sorted, keyed JSON-Lines file (one JSON object per line). Field names are configurable
    // so one implementation backs both source manifests (file/cook) and product indexes (path).
    class AssetIndexFile {
    public:
        AssetIndexFile(std::string keyField, std::string extraField = {});

        void Parse(const std::string &text);
        std::string Serialize() const;

        const IndexFileEntry *Find(std::string_view key) const;
        void Set(const IndexFileEntry &entry);
        bool Remove(std::string_view key);

        const std::vector<IndexFileEntry> &Entries() const { return entries; }
        bool Empty() const { return entries.empty(); }

        static AssetIndexFile Load(const FileSystemPtr &fs, const FilePath &path, std::string keyField, std::string extraField = {});
        static bool Save(const FileSystemPtr &fs, const FilePath &path, const AssetIndexFile &file);

    private:
        std::string keyField;
        std::string extraField;
        std::vector<IndexFileEntry> entries; // sorted by key
    };

    // Parsed-file cache keyed by (filesystem, directory).
    class AssetIndexFileCache {
    public:
        AssetIndexFileCache(std::string fileName, std::string keyField, std::string extraField = {});

        const AssetIndexFile &Get(const FileSystemPtr &fs, const FilePath &dir);
        bool Save(const FileSystemPtr &fs, const FilePath &dir, const AssetIndexFile &file);
        void Set(const FileSystemPtr &fs, const FilePath &dir, AssetIndexFile file);
        void Invalidate(const FileSystemPtr &fs, const FilePath &dir);
        void Clear();

    private:
        std::string MakeKey(const FileSystemPtr &fs, const FilePath &dir) const;

        std::string fileName;
        std::string keyField;
        std::string extraField;
        std::mutex mutex;
        std::unordered_map<std::string, AssetIndexFile> files;
    };

    // Canonical logical path (forward slashes) used as an index key across platforms.
    std::string MakeCanonicalPath(std::string path);

} // namespace sky
