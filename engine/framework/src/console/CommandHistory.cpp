//
// Created by blues on 2025/5/25.
//

#include <framework/console/CommandHistory.h>
#include <fstream>
#include <filesystem>

namespace sky {

    void CommandHistory::Add(const std::string &cmd)
    {
        if (cmd.empty()) {
            return;
        }
        // avoid consecutive duplicates
        if (!entries_.empty() && entries_.back() == cmd) {
            return;
        }
        entries_.push_back(cmd);
        if (entries_.size() > MAX_ENTRIES) {
            entries_.erase(entries_.begin());
        }
        cursor_ = -1;
    }

    std::string_view CommandHistory::GetPrevious()
    {
        if (entries_.empty()) {
            return {};
        }
        if (cursor_ < 0) {
            cursor_ = static_cast<int>(entries_.size()) - 1;
        } else if (cursor_ > 0) {
            --cursor_;
        }
        return entries_[cursor_];
    }

    std::string_view CommandHistory::GetNext()
    {
        if (cursor_ < 0 || entries_.empty()) {
            return {};
        }
        ++cursor_;
        if (cursor_ >= static_cast<int>(entries_.size())) {
            cursor_ = -1;
            return {};
        }
        return entries_[cursor_];
    }

    void CommandHistory::ResetCursor()
    {
        cursor_ = -1;
    }

    bool CommandHistory::Save(const std::string &path) const
    {
        auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        for (const auto &entry : entries_) {
            file << entry << '\n';
        }
        return true;
    }

    bool CommandHistory::Load(const std::string &path)
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        entries_.clear();
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                entries_.push_back(std::move(line));
            }
        }
        // keep only the most recent MAX_ENTRIES
        if (entries_.size() > MAX_ENTRIES) {
            entries_.erase(entries_.begin(), entries_.begin() + static_cast<int>(entries_.size() - MAX_ENTRIES));
        }
        cursor_ = -1;
        return true;
    }

} // namespace sky
