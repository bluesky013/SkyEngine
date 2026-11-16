//
// Created by blues on 2026/10/2.
//

#pragma once

#include <framework/asset/AssetProduct.h>
#include <string>
#include <vector>

namespace sky {

    // Split JSON Lines text into trimmed, non-empty lines.
    std::vector<std::string> SplitJsonLines(const std::string &text);

    struct AssetFileWriteResult {
        FileOperationStatus             status;
        std::vector<AssetRecoveryIssue> recovery;
        explicit                        operator bool() const
        {
            return static_cast<bool>(status) && recovery.empty();
        }
    };

    // Atomic replacement never publishes a failed candidate; recovery identifies retained temporary files.
    AssetFileWriteResult SaveAtomic(const FileSystemPtr &fs, const FilePath &path, const std::string &text);

} // namespace sky
