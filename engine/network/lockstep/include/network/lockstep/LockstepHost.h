//
// Created on 2026/10/01.
//

#pragma once

#include <network/lockstep/ILockstepSimulation.h>
#include <network/lockstep/LockstepConfig.h>
#include <network/lockstep/LockstepTypes.h>

#include <network/ConnectionId.h>
#include <network/NetworkHost.h>
#include <network/replication/ReplicationTick.h>

#include <cstdint>
#include <deque>
#include <span>
#include <unordered_map>
#include <vector>

namespace sky::net {

    // Host-authoritative deterministic lockstep over a NetworkHost. Peers exchange per-frame inputs on a
    // reliable-ordered channel; the authority assembles and broadcasts the authoritative input frame and
    // peers advance a fixed deterministic tick. Supports configurable input delay, predicted-input rollback,
    // and hash-based divergence detection with resynchronization.
    class LockstepHost {
    public:
        LockstepHost(NetworkHost &host, ILockstepSimulation &sim, const LockstepConfig &config);
        ~LockstepHost();

        // Validates the configuration (deterministic math mode required) and installs host handlers.
        LockstepResult Start();

        void SetAuthority(bool isAuthority) { authority = isAuthority; }
        void SetPlayerCount(uint32_t count) { playerCount = count; }
        void SetLocalPlayer(LockstepPlayerId player) { localPlayer = player; }

        // Queue a local input for a future frame (current frame + input delay).
        void SubmitInput(std::span<const uint8_t> input);

        void Update(double deltaSeconds);

        LockstepFrame CurrentFrame() const { return currentFrame; }
        LockstepFrame NextSubmitFrame() const { return nextSubmitFrame; }
        uint64_t      LastStateHash() const { return lastStateHash; }
        uint32_t      DesyncCount() const { return desyncCount; }
        uint32_t      ResyncCount() const { return resyncCount; }
        bool          RollbackEnabled() const { return config.enablePrediction; }
        bool          IsWaitingForInputs() const { return waitingForInputs; }

        // Correct a past frame's input: roll back, apply, and resimulate to the present. Used for predicted
        // input reconciliation and late corrections.
        LockstepResult ApplyCorrection(LockstepFrame frame, LockstepPlayerId player, std::span<const uint8_t> input);

    private:
        struct FrameRecord {
            LockstepFrame                     frame = 0;
            std::vector<std::vector<uint8_t>> inputs;
            std::vector<uint8_t>              stateBefore;
            uint64_t                          hashAfter = 0;
        };

        void HandleConnect(ConnectionId id);
        void HandleDisconnect(ConnectionId id);
        void HandleMessage(ConnectionId id, ChannelId channel, MessageSequence sequence, std::span<const uint8_t> payload);
        void HandleInputMessage(ConnectionId id, std::span<const uint8_t> payload);
        void HandleFrameMessage(std::span<const uint8_t> payload);
        void HandleHashMessage(std::span<const uint8_t> payload);
        void HandleResyncState(std::span<const uint8_t> payload);

        void AdvanceFrame(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &inputs);
        void BroadcastFrame(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &inputs);
        void BroadcastHash(LockstepFrame frame, uint64_t hash);
        void RequestResync();
        void SendResyncState(ConnectionId id);
        void TrimRing();

        FrameRecord   *FindFrame(LockstepFrame frame);
        bool           ReadyToAdvance(LockstepFrame frame) const;
        LockstepResult RollbackTo(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &correctedInputs);

        NetworkHost       &host;
        ILockstepSimulation &sim;
        LockstepConfig     config;
        ReplicationTick    tick;

        bool             authority = false;
        bool             deterministicMode = false;
        bool             waitingForInputs = false;
        uint32_t         playerCount = 1;
        LockstepPlayerId localPlayer = 0;
        LockstepPlayerId nextAssignedPlayer = 1;

        LockstepFrame currentFrame = 0;
        LockstepFrame nextSubmitFrame = 0;
        uint64_t      lastStateHash = 0;
        uint32_t      desyncCount = 0;
        uint32_t      resyncCount = 0;

        std::unordered_map<ConnectionId, LockstepPlayerId, ConnectionIdHash> playerOf;      // authority
        std::unordered_map<LockstepPlayerId, ConnectionId>                   playerConn;    // authority
        std::unordered_map<LockstepFrame, std::vector<std::vector<uint8_t>>> collected;     // authority
        std::unordered_map<LockstepFrame, std::vector<bool>>                 collectedPresent; // authority
        std::unordered_map<LockstepFrame, uint32_t>                          collectedCount; // authority
        std::unordered_map<LockstepFrame, std::vector<uint8_t>>             localInputs;    // all peers
        std::unordered_map<LockstepFrame, std::vector<std::vector<uint8_t>>> incoming;      // client
        std::unordered_map<LockstepFrame, uint64_t>                         authoritativeHashes; // client
        std::vector<std::vector<uint8_t>>                                    lastKnownInputs;     // client
        ConnectionId                                                         authorityConn = INVALID_CONNECTION_ID; // client

        std::deque<FrameRecord> ring;
    };

} // namespace sky::net
