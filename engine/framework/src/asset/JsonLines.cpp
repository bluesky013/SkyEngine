//
// Created by blues on 2026/10/2.
//

#include <core/util/Uuid.h>
#include <framework/asset/JsonLines.h>

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

    bool SaveAtomic(const FileSystemPtr &fs, const FilePath &path, const std::string &text)
    {
        if (fs == nullptr) {
            return false;
        }

        const FilePath tempPath(path.GetStr() + "." + Uuid::Create().ToString() + ".tmp");
        if (!fs->WriteFileExclusive(tempPath, {reinterpret_cast<const uint8_t *>(text.data()), text.size()})) {
            return false;
        }
        return static_cast<bool>(fs->ReplaceFileAtomic(tempPath, path));
    }

} // namespace sky
