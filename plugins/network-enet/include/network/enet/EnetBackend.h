//
// Created on 2026/09/25.
//

#pragma once

#include <network/INetBackend.h>

#include <cstdint>
#include <memory>

namespace sky::net {

    // First real transport backend: reliable/unreliable channels over UDP using ENet.
    class EnetBackend final : public INetBackend {
    public:
        EnetBackend();
        ~EnetBackend() override;

        const NetworkBackendCaps &GetCaps() const override;
        bool Init() override;
        void Shutdown() override;
        INetListener *Listen(const ListenDesc &desc) override;
        ConnectionId  Connect(const ConnectDesc &desc) override;
        uint32_t Pump(INetEventSink &sink, uint32_t maxEvents, uint32_t waitMs) override;
        INetConnection *GetConnection(ConnectionId id) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

} // namespace sky::net
