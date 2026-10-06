//
// Created on 2026/10/06.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sky::editor {

    enum class FileBrowserMode : uint8_t {
        OPEN_FILE = 0,
        OPEN_PROJECT,
        SELECT_DIRECTORY,
    };

    // A named filter. It matches by file extension (filesystem sources) and/or by
    // asset type id (asset sources). Empty lists mean "match anything".
    struct FileBrowserFilter {
        std::string              label;
        std::vector<std::string> extensions; // lowercase, dot-free, e.g. "skyproj"
        std::vector<std::string> assetTypes; // asset type ids, e.g. "Texture"
    };

    // A sidebar shortcut (Home, Project, a drive, ...).
    struct FileBrowserPlace {
        std::string label;
        std::string location;
    };

    struct FileBrowserEntry {
        std::string name;
        std::string path; // source-native full location
        bool        isDirectory = false;
        std::string typeId; // asset type id (empty for filesystem files)
    };

    struct FileBrowserRequest {
        FileBrowserMode                mode = FileBrowserMode::OPEN_FILE;
        std::string                    title;
        std::string                    directory;
        std::vector<FileBrowserFilter> filters;
        std::vector<FileBrowserPlace>  places; // empty -> taken from the source
        std::string                    defaultName;
    };

    struct FileBrowserResult {
        bool        accepted  = false;
        bool        directory = false;
        std::string path;
    };

} // namespace sky::editor
