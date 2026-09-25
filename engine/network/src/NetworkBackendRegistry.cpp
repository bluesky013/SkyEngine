//
// Created on 2026/09/25.
//

#include <network/NetworkBackendRegistry.h>

namespace sky::net {

    NetworkBackendRegistry &NetworkBackendRegistry::Get()
    {
        static NetworkBackendRegistry instance;
        return instance;
    }

    NetworkBackendRegistry::~NetworkBackendRegistry()
    {
        Clear();
    }

    bool NetworkBackendRegistry::Register(NetworkRole role, INetBackend *backend)
    {
        if (role >= NetworkRole::Count || backend == nullptr) {
            return false;
        }

        Unregister(role);

        if (!backend->Init()) {
            delete backend;
            return false;
        }
        backends[static_cast<size_t>(role)] = backend;
        return true;
    }

    void NetworkBackendRegistry::Unregister(NetworkRole role)
    {
        if (role >= NetworkRole::Count) {
            return;
        }
        auto *&slot = backends[static_cast<size_t>(role)];
        if (slot != nullptr) {
            slot->Shutdown();
            delete slot;
            slot = nullptr;
        }
    }

    void NetworkBackendRegistry::Clear()
    {
        for (size_t i = 0; i < backends.size(); ++i) {
            auto *backend = backends[i];
            if (backend != nullptr) {
                backend->Shutdown();
                delete backend;
                backends[i] = nullptr;
            }
        }
    }

    bool NetworkBackendRegistry::Has(NetworkRole role) const
    {
        return role < NetworkRole::Count && backends[static_cast<size_t>(role)] != nullptr;
    }

    INetBackend *NetworkBackendRegistry::Get(NetworkRole role) const
    {
        if (role >= NetworkRole::Count) {
            return nullptr;
        }
        return backends[static_cast<size_t>(role)];
    }

} // namespace sky::net
