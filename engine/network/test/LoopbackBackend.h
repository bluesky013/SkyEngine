//
// Created on 2026/09/25.
//

#pragma once

#include <network/INetBackend.h>

#include <cstdint>

namespace sky::net::test {

    // A deterministic in-process transport used by the test suite. Connect pairs a client connection
    // with a server connection on a registered listener; messages are delivered FIFO on the next Pump.
    class LoopbackBackend final : public INetBackend {
    public:
        LoopbackBackend();
        ~LoopbackBackend() override;

        const NetworkBackendCaps &GetCaps() const override;
        bool Init() override;
        void Shutdown() override;
        INetListener *Listen(const ListenDesc &desc) override;
        ConnectionId  Connect(const ConnectDesc &desc) override;
        uint32_t Pump(INetEventSink &sink, uint32_t maxEvents, uint32_t waitMs) override;
        INetConnection *GetConnection(ConnectionId id) override;

        // Deterministic loss: drop every Nth send. 0 disables loss.
        void     SetDropEveryN(uint32_t n) { dropEveryN = n; }
        uint32_t GetDroppedCount() const { return dropped; }

        void ForceDisconnect(ConnectionId id, DisconnectReason reason);
        void SetRtt(ConnectionId id, uint32_t ms);

        // Capability tuning for tests (for example disabling unreliable delivery).
        void SetUnreliableSupported(bool value) { caps.unreliable = value; }

        // Returns true when the next send should be dropped (deterministic loss).
        bool ShouldDropNextSend();

    private:
        static NetworkBackendCaps MakeDefaultCaps();

        NetworkBackendCaps caps = MakeDefaultCaps();
        uint32_t dropEveryN  = 0;
        uint32_t sendCounter = 0;
        uint32_t dropped     = 0;
    };

} // namespace sky::net::test
