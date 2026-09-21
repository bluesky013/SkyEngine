//
// Created on 2026/09/21.
//

#include <editor/core/log/LogService.h>
#include <core/logger/Logger.h>
#include <chrono>
#include <utility>

namespace sky::editor {

    namespace {

        std::string ToLower(std::string value)
        {
            for (char &ch : value) {
                if (ch >= 'A' && ch <= 'Z') {
                    ch = static_cast<char>(ch - 'A' + 'a');
                }
            }
            return value;
        }

        bool ContainsIgnoreCase(const std::string &haystack, const std::string &needle)
        {
            if (needle.empty()) {
                return true;
            }
            return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
        }

    } // namespace

    LogService::~LogService()
    {
        Uninstall();
    }

    void LogService::Install()
    {
        if (installed) {
            return;
        }
        Logger::SetOutputCallback([this](const char *tag, const char *level, const char *message) {
            OnLogOutput(tag, level, message);
        });
        installed = true;
    }

    void LogService::Uninstall()
    {
        if (!installed) {
            return;
        }
        Logger::SetOutputCallback(nullptr);
        installed = false;
    }

    void LogService::OnLogOutput(const char *tag, const char *level, const char *message)
    {
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        Entry entry;
        entry.tag = tag != nullptr ? tag : "";
        entry.level = level != nullptr ? level : "";
        entry.message = message != nullptr ? message : "";
        entry.timestamp = static_cast<uint64_t>(now);

        std::lock_guard<std::mutex> lock(mutex);
        ring.push_back(std::move(entry));
        while (ring.size() > kCapacity) {
            ring.pop_front();
        }
        dirty = true;
    }

    void LogService::Pump()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!dirty) {
                return;
            }
        }
        Rebuild();
    }

    size_t LogService::GetTotalCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return ring.size();
    }

    void LogService::Clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        ring.clear();
        visible.clear();
        dirty = false;
    }

    void LogService::SetLevelFilter(std::string level)
    {
        levelFilter = std::move(level);
        dirty = true;
        Rebuild();
    }

    void LogService::SetTagFilter(std::string tag)
    {
        tagFilter = std::move(tag);
        dirty = true;
        Rebuild();
    }

    void LogService::SetSearch(std::string text)
    {
        search = std::move(text);
        dirty = true;
        Rebuild();
    }

    void LogService::ResetFilters()
    {
        levelFilter.clear();
        tagFilter.clear();
        search.clear();
        dirty = true;
        Rebuild();
    }

    bool LogService::Match(const Entry &entry) const
    {
        if (!levelFilter.empty() && entry.level != levelFilter) {
            return false;
        }
        if (!tagFilter.empty() && !ContainsIgnoreCase(entry.tag, tagFilter)) {
            return false;
        }
        if (!search.empty() && !ContainsIgnoreCase(entry.message, search) && !ContainsIgnoreCase(entry.tag, search)) {
            return false;
        }
        return true;
    }

    void LogService::Rebuild()
    {
        std::lock_guard<std::mutex> lock(mutex);
        visible.clear();
        for (const auto &entry : ring) {
            if (Match(entry)) {
                visible.push_back(entry);
            }
        }
        dirty = false;
    }

} // namespace sky::editor
