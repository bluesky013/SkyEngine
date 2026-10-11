//
// Created on 2026/09/25.
//

#pragma once

#include <network/INetBackend.h>
#include <network/NetworkCaps.h>

#include <core/environment/Singleton.h>

#include <array>

namespace sky::net {

    // Registry of active backends keyed by role. Unlike the physics registry, several backends may be
    // active at once (for example Server and Cluster), so registration is per role.
    //
    // Process-wide (cross-DLL): engine/network is a static lib linked into the backend plugin and the
    // hosts, so the instance lives in the shared Environment (Singleton), not a function-local static.
    class NetworkBackendRegistry : public Singleton<NetworkBackendRegistry> {
    public:
        static NetworkBackendRegistry &Get();

        // Takes ownership of the backend. Shuts down and releases any backend previously registered for
        // the role. Returns false when the backend is null or fails to initialize.
        bool Register(NetworkRole role, INetBackend *backend);
        void Unregister(NetworkRole role);
        void Clear();

        bool         Has(NetworkRole role) const;
        INetBackend *Get(NetworkRole role) const;

        ~NetworkBackendRegistry() override;

    private:
        friend class Singleton<NetworkBackendRegistry>;

        NetworkBackendRegistry() = default;

        static constexpr size_t               ROLE_COUNT = static_cast<size_t>(NetworkRole::Count);
        std::array<INetBackend *, ROLE_COUNT> backends{};
    };

} // namespace sky::net
