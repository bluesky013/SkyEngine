//
// Created on 2026/09/25.
//

#include <network/enet/EnetBackend.h>
#include <network/NetworkBackendRegistry.h>

#include <framework/interface/IModule.h>

namespace sky::net {

    // Registers an ENet backend for the Client and Server roles. Runtime resolution goes through
    // NetworkBackendRegistry, so consumers never name the concrete backend.
    class EnetNetworkModule : public IModule {
    public:
        EnetNetworkModule() = default;
        ~EnetNetworkModule() override = default;

        bool Init(const StartArguments &) override { return true; }

        void Start() override
        {
            auto &registry = NetworkBackendRegistry::Get();
            registry.Register(NetworkRole::Client, new EnetBackend());
            registry.Register(NetworkRole::Server, new EnetBackend());
        }

        void Shutdown() override
        {
            auto &registry = NetworkBackendRegistry::Get();
            registry.Unregister(NetworkRole::Client);
            registry.Unregister(NetworkRole::Server);
        }
    };

} // namespace sky::net

REGISTER_MODULE(sky::net::EnetNetworkModule)
