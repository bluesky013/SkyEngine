//
// Created by blues on 2025/5/25.
//

#include <framework/console/ConsoleLog.h>
#include <core/logger/Logger.h>
#include <chrono>

namespace sky {

    ConsoleLog::~ConsoleLog()
    {
        Uninstall();
    }

    void ConsoleLog::Install()
    {
        if (installed_) {
            return;
        }
        ring_.resize(RING_SIZE);
        Logger::SetOutputCallback([this](const char *tag, const char *type, const char *message) {
            OnLogOutput(tag, type, message);
        });
        installed_ = true;
    }

    void ConsoleLog::Uninstall()
    {
        if (!installed_) {
            return;
        }
        Logger::SetOutputCallback(nullptr);
        installed_ = false;
    }

    void ConsoleLog::OnLogOutput(const char *tag, const char *type, const char *message)
    {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        LogEntry entry;
        entry.tag = tag;
        entry.level = type;
        entry.message = message;
        entry.timestamp = static_cast<uint64_t>(now);

        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(entry);

        // also write to ring buffer
        ring_[ringHead_] = entry;
        ringHead_ = (ringHead_ + 1) % RING_SIZE;
        if (ringCount_ < RING_SIZE) {
            ++ringCount_;
        }
    }

    void ConsoleLog::FlushPendingEntries(const std::function<void(const LogEntry &)> &callback)
    {
        std::vector<LogEntry> flushed;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            flushed.swap(pending_);
        }
        for (const auto &entry : flushed) {
            callback(entry);
        }
    }

} // namespace sky
