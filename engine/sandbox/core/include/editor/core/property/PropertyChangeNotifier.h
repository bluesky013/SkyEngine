//
// Created on 2026/10/04.
//

#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Revision + callback hub for reflected-data changes made outside the editor
    // (gameplay systems, tools, async loaders). A view keeps the last revision it
    // saw and rebuilds when it changes; a data owner calls Notify() after writing.
    class PropertyChangeNotifier {
    public:
        using Callback = std::function<void()>;

        uint64_t GetRevision() const { return revision; }

        int Subscribe(Callback callback)
        {
            const int id = nextId++;
            callbacks.emplace(id, std::move(callback));
            return id;
        }

        void Unsubscribe(int id) { callbacks.erase(id); }

        void Notify()
        {
            ++revision;
            std::vector<Callback> snapshot;
            snapshot.reserve(callbacks.size());
            for (auto &entry : callbacks) {
                snapshot.push_back(entry.second);
            }
            for (auto &callback : snapshot) {
                if (callback) {
                    callback();
                }
            }
        }

    private:
        uint64_t                                  revision = 0;
        int                                       nextId = 1;
        std::unordered_map<int, Callback>         callbacks;
    };

} // namespace sky::editor
