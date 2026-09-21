//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace sky::editor {

    // Toolkit-independent editor log service.
    //
    // Hooks the engine logger (any thread), captures entries into a bounded
    // ring, and exposes a filtered snapshot rebuilt on the main thread via
    // Pump(). A panel renders GetVisibleEntries(); no UI/render dependency.
    class LogService {
    public:
        struct Entry {
            std::string tag;
            std::string level;
            std::string message;
            uint64_t timestamp = 0;
        };

        static constexpr size_t kCapacity = 2048;

        LogService() = default;
        ~LogService();

        LogService(const LogService &) = delete;
        LogService &operator=(const LogService &) = delete;

        void Install();
        void Uninstall();
        bool IsInstalled() const { return installed; }

        // Main thread: refresh the filtered view from captured entries.
        void Pump();

        const std::vector<Entry> &GetVisibleEntries() const { return visible; }
        size_t GetTotalCount() const;
        void Clear();

        void SetLevelFilter(std::string level);
        void SetTagFilter(std::string tag);
        void SetSearch(std::string text);
        void ResetFilters();

        const std::string &GetLevelFilter() const { return levelFilter; }
        const std::string &GetTagFilter() const { return tagFilter; }
        const std::string &GetSearch() const { return search; }

    private:
        void OnLogOutput(const char *tag, const char *level, const char *message);
        bool Match(const Entry &entry) const;
        void Rebuild();

        mutable std::mutex mutex;
        std::deque<Entry> ring;
        bool dirty = true;
        bool installed = false;

        std::vector<Entry> visible;
        std::string levelFilter;
        std::string tagFilter;
        std::string search;
    };

} // namespace sky::editor
