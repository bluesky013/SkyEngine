//
// Created on 2026/09/22.
//

#include <core/environment/Singleton.h>
#include <physics/PhysicsMesh.h>

#include <utility>

namespace sky::phy {

    namespace {
        class PhysicsMeshProviderState final : public Singleton<PhysicsMeshProviderState> {
        public:
            PhysicsMeshProvider provider;
            std::string         assetType;
        };
    } // namespace

    void SetPhysicsMeshProvider(PhysicsMeshProvider provider, std::string assetType)
    {
        auto *state     = PhysicsMeshProviderState::Get();
        state->provider = std::move(provider);
        if (!assetType.empty())
            state->assetType = std::move(assetType);
    }

    std::string GetPhysicsMeshAssetType()
    {
        return PhysicsMeshProviderState::Get()->assetType;
    }

    CounterPtr<TriangleMesh> CreatePhysicsMesh(const Uuid &asset)
    {
        auto *state    = PhysicsMeshProviderState::Get();
        auto  provider = state->provider;
        return provider ? provider(asset) : nullptr;
    }

} // namespace sky::phy
