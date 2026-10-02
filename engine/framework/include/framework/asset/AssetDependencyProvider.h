//
// Created by blues on 2026/10/2.
//

#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <core/util/Uuid.h>

namespace sky {

    // Read-only view over the asset dependency graph.
    // Forward edges come from product headers (runtime) or source dependencies (editor);
    // reverse edges are obtained by inverting the forward graph.
    class IAssetDependencyProvider {
    public:
        virtual ~IAssetDependencyProvider() = default;

        virtual std::vector<Uuid> Dependencies(const Uuid &id) const = 0;
        virtual std::vector<Uuid> Dependents(const Uuid &id) const = 0;
        virtual void ForEach(const std::function<void(const Uuid &, const std::vector<Uuid> &)> &fn) const = 0;
    };

    // Concrete graph: stores forward edges; the reverse map is rebuilt lazily on demand.
    class AssetDependencyGraph : public IAssetDependencyProvider {
    public:
        AssetDependencyGraph() = default;
        ~AssetDependencyGraph() override = default;

        void Clear();
        void Add(const Uuid &id, const std::vector<Uuid> &dependencies);

        std::vector<Uuid> Dependencies(const Uuid &id) const override;
        std::vector<Uuid> Dependents(const Uuid &id) const override;
        void ForEach(const std::function<void(const Uuid &, const std::vector<Uuid> &)> &fn) const override;

    private:
        void EnsureReverse() const;

        std::unordered_map<Uuid, std::vector<Uuid>> forward;

        mutable bool reverseDirty = true;
        mutable std::unordered_map<Uuid, std::vector<Uuid>> reverse;
    };

} // namespace sky
