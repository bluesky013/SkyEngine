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
            std::shared_ptr<const void> ownerLease;
            std::shared_ptr<const void> providerLease;
            PhysicsMeshProvider         provider;
            std::string                 assetType;
        };
    } // namespace

    void SetPhysicsMeshProvider(PhysicsMeshProvider provider, std::shared_ptr<const void> codeLease, std::string assetType)
    {
        auto *state = PhysicsMeshProviderState::Get();
        if (!state->ownerLease) {
            state->ownerLease = codeLease;
        }
        state->provider      = std::move(provider);
        state->providerLease = std::move(codeLease);
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
        auto  lease    = state->providerLease;
        auto  provider = state->provider;
        return provider ? provider(asset) : nullptr;
    }

} // namespace sky::phy
