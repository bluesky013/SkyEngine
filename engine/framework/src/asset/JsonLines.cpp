//
// Created by blues on 2026/10/2.
//

#include <framework/asset/JsonLines.h>

#include <core/util/Uuid.h>

namespace sky {

    std::vector<std::string> SplitJsonLines(const std::string &text)
    {
        std::vector<std::string> lines;

        size_t begin = 0;
        while (begin < text.size()) {
            auto end = text.find('\n', begin);
            if (end == std::string::npos) {
                end = text.size();
            }

            auto line = text.substr(begin, end - begin);
            begin     = end + 1;

            auto first = line.find_first_not_of(" \t\r");
            if (first == std::string::npos) {
                continue;
            }
            auto last = line.find_last_not_of(" \t\r");
            lines.emplace_back(line.substr(first, last - first + 1));
        }
        return lines;
    }

    AssetFileWriteResult SaveAtomic(const FileSystemPtr &fs, const FilePath &path, const std::string &text)
    {
        AssetFileWriteResult result;
        if (!fs) {
            result.status = {FileOperationError::InvalidPath, "Atomic write requires a filesystem"};
            return result;
        }
        const FilePath temporary(path.GetStr() + "." + Uuid::Create().ToString() + ".tmp");
        result.status = fs->WriteFileExclusive(temporary, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
        if (!result.status) {
            // A failed exclusive write does not prove ownership of a surviving path.
            if (result.status.error != FileOperationError::AlreadyExists && fs->FileExist(temporary))
                result.recovery.push_back({"write-temporary", fs->GetPath() / temporary, result.status});
            return result;
        }
        result.status = fs->ReplaceFileAtomic(temporary, path);
        if (!result.status) {
            if (const auto cleanup = fs->RemoveFile(temporary); !cleanup)
                result.recovery.push_back({"remove-temporary", fs->GetPath() / temporary, cleanup});
        }
        return result;
    }

} // namespace sky
