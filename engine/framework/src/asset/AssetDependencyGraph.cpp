//
// Created by blues on 2026/10/2.
//

#include <framework/asset/AssetDependencyProvider.h>

namespace sky {

    void AssetDependencyGraph::Clear()
    {
        forward.clear();
        reverse.clear();
        reverseDirty = true;
    }

    void AssetDependencyGraph::Add(const Uuid &id, const std::vector<Uuid> &dependencies)
    {
        forward[id] = dependencies;
        reverseDirty = true;
    }

    std::vector<Uuid> AssetDependencyGraph::Dependencies(const Uuid &id) const
    {
        auto iter = forward.find(id);
        return iter != forward.end() ? iter->second : std::vector<Uuid>{};
    }

    void AssetDependencyGraph::EnsureReverse() const
    {
        if (!reverseDirty) {
            return;
        }

        reverse.clear();
        for (const auto &[id, deps] : forward) {
            for (const auto &dep : deps) {
                reverse[dep].push_back(id);
            }
        }
        reverseDirty = false;
    }

    std::vector<Uuid> AssetDependencyGraph::Dependents(const Uuid &id) const
    {
        EnsureReverse();
        auto iter = reverse.find(id);
        return iter != reverse.end() ? iter->second : std::vector<Uuid>{};
    }

    void AssetDependencyGraph::ForEach(const std::function<void(const Uuid &, const std::vector<Uuid> &)> &fn) const
    {
        for (const auto &[id, deps] : forward) {
            fn(id, deps);
        }
    }

} // namespace sky
