//
// Created by blues on 2026/10/2.
//

#pragma once

#include <string>
#include <vector>
#include <core/file/FileSystem.h>

namespace sky {

    // Split JSON Lines text into trimmed, non-empty lines.
    std::vector<std::string> SplitJsonLines(const std::string &text);

    // Atomic write (temp file + rename) of text to <fs>/<path>.
    bool SaveAtomic(const FileSystemPtr &fs, const FilePath &path, const std::string &text);

} // namespace sky
