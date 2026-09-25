//
// Created on 2026/09/25.
//

#pragma once

#include <network/INetBackend.h>
#include <network/NetworkCaps.h>

#include <array>

namespace sky::net {

    // Registry of active backends keyed by role. Unlike the physics registry, several backends may be
    // active at once (for example Server and Cluster), so registration is per role.
    class NetworkBackendRegistry {
    public:
        static NetworkBackendRegistry &Get();

        // Takes ownership of the backend. Shuts down and releases any backend previously registered for
        // the role. Returns false when the backend is null or fails to initialize.
        bool Register(NetworkRole role, INetBackend *backend);
        void Unregister(NetworkRole role);
        void Clear();

        bool         Has(NetworkRole role) const;
        INetBackend *Get(NetworkRole role) const;

        NetworkBackendRegistry()  = default;
        ~NetworkBackendRegistry();

    private:
        static constexpr size_t ROLE_COUNT = static_cast<size_t>(NetworkRole::Count);
        std::array<INetBackend *, ROLE_COUNT> backends{};
    };

} // namespace sky::net
