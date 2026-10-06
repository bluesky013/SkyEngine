//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/filebrowser/IFileBrowserSource.h>

namespace sky::editor {

    // std::filesystem-backed IFileBrowserSource. Places: the user home plus any
    // available drives (Windows).
    class FileSystemSource : public IFileBrowserSource {
    public:
        std::string Root() const override;
        std::string Parent(const std::string &location) const override;
        std::string Join(const std::string &location, const std::string &name) const override;
        bool        IsDirectory(const std::string &location) const override;
        bool        List(const std::string &location, std::vector<FileBrowserEntry> &out, std::string &error) const override;
        bool        CreateDirectory(const std::string &location, const std::string &name, std::string &error) override;
        bool        Rename(const std::string &location, const std::string &oldName, const std::string &newName, std::string &error) override;
        std::vector<FileBrowserPlace> Places() const override;
    };

} // namespace sky::editor
