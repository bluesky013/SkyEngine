#pragma once

#include <editor/core/preferences/PreferencesPersistence.h>

#include <filesystem>

namespace sky::editor {

    // Binary file storage. Write completes and closes an exclusive temporary
    // file in the destination directory before atomically replacing the path.
    // Failed writes preserve the previous file and remove their temporary file.
    // Calls are serialized by the owner; no cross-writer CAS or power-loss
    // durability is promised. JSON validation belongs to PreferencesPersistence.
    class FilePreferenceStorage final : public IPreferenceStorage {
    public:
        explicit FilePreferenceStorage(std::filesystem::path path);

        PreferenceResult             Read(std::string &document) override;
        PreferenceResult             Write(const std::string &document) override;
        const std::filesystem::path &GetPath() const;

    private:
        std::filesystem::path path;
    };

} // namespace sky::editor
