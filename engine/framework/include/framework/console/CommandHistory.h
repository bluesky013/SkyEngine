//
// Created by blues on 2025/5/25.
//

#pragma once

#include <string>
#include <vector>

namespace sky {

    class CommandHistory {
    public:
        static constexpr size_t MAX_ENTRIES = 1000;

        CommandHistory() = default;
        ~CommandHistory() = default;

        void Add(const std::string &cmd);

        std::string_view GetPrevious();
        std::string_view GetNext();
        void ResetCursor();

        bool Save(const std::string &path) const;
        bool Load(const std::string &path);

        size_t Size() const { return entries_.size(); }

    private:
        std::vector<std::string> entries_;
        int cursor_ = -1;
    };

} // namespace sky
