//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/filebrowser/FileBrowserTypes.h>

#include <string>
#include <vector>

namespace sky::editor {

    // A location provider for the file browser. The filesystem source lists real
    // directories; future asset sources list asset-database nodes (and fill in
    // `typeId` so the browser can filter by asset type). Locations are opaque
    // strings owned by the source.
    class IFileBrowserSource {
    public:
        virtual ~IFileBrowserSource() = default;

        virtual std::string Root() const                                                     = 0;
        virtual std::string Parent(const std::string &location) const                        = 0;
        virtual std::string Join(const std::string &location, const std::string &name) const = 0;
        virtual bool        IsDirectory(const std::string &location) const                   = 0;

        // Lists the (unfiltered) children of a directory. Returns false and sets
        // `error` on failure, leaving `out` unspecified.
        virtual bool List(const std::string &location, std::vector<FileBrowserEntry> &out, std::string &error) const = 0;

        // Creates a child directory. Read-only sources may decline (default).
        virtual bool CreateDirectory(const std::string &location, const std::string &name, std::string &error)
        {
            (void)location;
            (void)name;
            error = "Creating folders is not supported here";
            return false;
        }

        // Renames a child entry in place. Read-only sources may decline (default).
        virtual bool Rename(const std::string &location, const std::string &oldName, const std::string &newName, std::string &error)
        {
            (void)location;
            (void)oldName;
            (void)newName;
            error = "Renaming is not supported here";
            return false;
        }

        // Optional sidebar shortcuts. Default: none.
        virtual std::vector<FileBrowserPlace> Places() const
        {
            return {};
        }
    };

} // namespace sky::editor
