//
// Created by blues on 2025/5/25.
//

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace sky {

    struct LogEntry {
        std::string tag;
        std::string level;
        std::string message;
        uint64_t    timestamp = 0;
    };

    class ConsoleLog {
    public:
        static constexpr size_t RING_SIZE = 2048;

        ConsoleLog() = default;
        ~ConsoleLog();

        void Install();
        void Uninstall();

        void FlushPendingEntries(const std::function<void(const LogEntry &)> &callback);

    private:
        void OnLogOutput(const char *tag, const char *type, const char *message);

        std::mutex              mutex_;
        std::vector<LogEntry>   pending_;
        bool                    installed_ = false;

        // ring buffer for history
        std::vector<LogEntry>   ring_;
        size_t                  ringHead_ = 0;
        size_t                  ringCount_ = 0;
    };

} // namespace sky
