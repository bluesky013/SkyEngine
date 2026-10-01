//
// Created on 2026/10/01.
//

#include <network/lockstep/LockstepHost.h>

#include <network/detail/ByteCodec.h>

#include <algorithm>

namespace sky::net {

    LockstepHost::LockstepHost(NetworkHost &host, ILockstepSimulation &sim, const LockstepConfig &config)
        : host(host), sim(sim), config(config), tick(config.tickRateHz)
    {
        if (playerCount > 0) {
            lastKnownInputs.resize(playerCount);
        }
        host.SetConnectHandler([this](ConnectionId id) { HandleConnect(id); });
        host.SetDisconnectHandler([this](ConnectionId id, DisconnectReason) { HandleDisconnect(id); });
        host.SetMessageHandler([this](ConnectionId id, ChannelId channel, MessageSequence sequence,
                                      std::span<const uint8_t> payload) {
            HandleMessage(id, channel, sequence, payload);
        });
    }

    LockstepHost::~LockstepHost() = default;

    LockstepResult LockstepHost::Start()
    {
        const LockstepResult result = ValidateLockstepConfig(config);
        deterministicMode = (result == LockstepResult::Ok);
        nextSubmitFrame = config.inputDelayFrames;
        lastKnownInputs.assign(playerCount, {});
        return result;
    }

    void LockstepHost::SubmitInput(std::span<const uint8_t> input)
    {
        const LockstepFrame frame = nextSubmitFrame++;
        std::vector<uint8_t> bytes(input.begin(), input.end());
        localInputs[frame] = bytes;

        if (authority) {
            auto &slot = collected[frame];
            slot.resize(playerCount);
            auto &present = collectedPresent[frame];
            present.resize(playerCount, false);
            slot[localPlayer] = bytes;
            if (!present[localPlayer]) {
                present[localPlayer] = true;
                ++collectedCount[frame];
            }
        } else if (IsValid(authorityConn)) {
            std::vector<uint8_t> message;
            ByteWriter writer(message);
            writer.U8(static_cast<uint8_t>(LockstepMessage::Input));
            writer.U64(frame);
            writer.U16(static_cast<uint16_t>(bytes.size()));
            writer.Bytes(bytes);
            host.Send(authorityConn, LOCKSTEP_CHANNEL, message, DeliveryMode::ReliableOrdered);
        }
    }

    void LockstepHost::Update(double deltaSeconds)
    {
        if (authority) {
            const uint32_t ticks = tick.Advance(deltaSeconds);
            for (uint32_t i = 0; i < ticks; ++i) {
                const LockstepFrame frame = currentFrame;
                if (!ReadyToAdvance(frame)) {
                    waitingForInputs = true;
                    break;
                }
                const auto inputs = collected[frame];
                AdvanceFrame(frame, inputs);
                BroadcastFrame(frame, inputs);
                if (config.hashCadenceFrames > 0 && (frame + 1) % config.hashCadenceFrames == 0) {
                    BroadcastHash(frame, lastStateHash);
                }
                collected.erase(frame);
                collectedCount.erase(frame);
                collectedPresent.erase(frame);
                ++currentFrame;
                waitingForInputs = false;
            }
            return;
        }

        if (config.enablePrediction) {
            // Advance locally using predicted remote inputs while we have our own input for the frame.
            while (true) {
                const LockstepFrame frame = currentFrame;
                auto local = localInputs.find(frame);
                if (local == localInputs.end()) {
                    waitingForInputs = true;
                    break;
                }
                std::vector<std::vector<uint8_t>> inputs(playerCount);
                for (LockstepPlayerId p = 0; p < playerCount; ++p) {
                    if (p == localPlayer) {
                        inputs[p] = local->second;
                    } else if (p < lastKnownInputs.size()) {
                        inputs[p] = lastKnownInputs[p];
                    }
                }
                AdvanceFrame(frame, inputs);
                ++currentFrame;
                waitingForInputs = false;
            }
            return;
        }

        // Non-predicting client: advance only when the authoritative frame for the next frame arrives.
        while (true) {
            auto it = incoming.find(currentFrame);
            if (it == incoming.end()) {
                waitingForInputs = true;
                break;
            }
            const LockstepFrame frame = currentFrame;
            const auto inputs = it->second;
            incoming.erase(it);
            AdvanceFrame(frame, inputs);
            ++currentFrame;
            waitingForInputs = false;
        }
    }

    LockstepResult LockstepHost::ApplyCorrection(LockstepFrame frame, LockstepPlayerId player,
                                                 std::span<const uint8_t> input)
    {
        if (!deterministicMode) {
            return LockstepResult::RollbackUnavailable;
        }
        if (currentFrame > frame && currentFrame - frame > config.rollbackMaxFrames) {
            RequestResync();
            return LockstepResult::RollbackDepthExceeded;
        }
        FrameRecord *record = FindFrame(frame);
        if (record == nullptr) {
            return LockstepResult::InvalidArgument;
        }
        std::vector<std::vector<uint8_t>> inputs = record->inputs;
        if (player >= inputs.size()) {
            inputs.resize(player + 1);
        }
        inputs[player].assign(input.begin(), input.end());
        return RollbackTo(frame, inputs);
    }

    void LockstepHost::AdvanceFrame(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &inputs)
    {
        FrameRecord record;
        record.frame  = frame;
        record.inputs = inputs;
        sim.CaptureState(record.stateBefore);
        sim.Advance(frame, inputs);
        record.hashAfter = sim.StateHash();
        lastStateHash = record.hashAfter;

        if (!authority) {
            auto hashIt = authoritativeHashes.find(frame);
            if (hashIt != authoritativeHashes.end()) {
                if (hashIt->second != record.hashAfter) {
                    ++desyncCount;
                    RequestResync();
                }
                authoritativeHashes.erase(hashIt);
            }
        }

        ring.push_back(std::move(record));
        TrimRing();
    }

    LockstepResult LockstepHost::RollbackTo(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &correctedInputs)
    {
        if (!deterministicMode) {
            return LockstepResult::RollbackUnavailable;
        }
        if (currentFrame > frame && currentFrame - frame > config.rollbackMaxFrames) {
            RequestResync();
            return LockstepResult::RollbackDepthExceeded;
        }
        FrameRecord *record = FindFrame(frame);
        if (record == nullptr) {
            return LockstepResult::InvalidArgument;
        }

        sim.RestoreState(record->stateBefore);
        for (LockstepFrame f = frame; f < currentFrame; ++f) {
            FrameRecord *r = FindFrame(f);
            std::vector<std::vector<uint8_t>> inputs = (f == frame) ? correctedInputs : (r ? r->inputs : correctedInputs);
            sim.Advance(f, inputs);
            if (r != nullptr) {
                r->inputs    = inputs;
                r->hashAfter = sim.StateHash();
            }
        }
        lastStateHash = sim.StateHash();
        return LockstepResult::Ok;
    }

    void LockstepHost::HandleMessage(ConnectionId id, ChannelId channel, MessageSequence, std::span<const uint8_t> payload)
    {
        if (channel != LOCKSTEP_CHANNEL || payload.empty()) {
            return;
        }
        switch (static_cast<LockstepMessage>(payload[0])) {
        case LockstepMessage::Input:
            HandleInputMessage(id, payload.subspan(1));
            break;
        case LockstepMessage::Frame:
            HandleFrameMessage(payload.subspan(1));
            break;
        case LockstepMessage::Hash:
            HandleHashMessage(payload.subspan(1));
            break;
        case LockstepMessage::ResyncRequest:
            SendResyncState(id);
            break;
        case LockstepMessage::ResyncState:
            HandleResyncState(payload.subspan(1));
            break;
        }
    }

    void LockstepHost::HandleConnect(ConnectionId id)
    {
        if (authority) {
            const LockstepPlayerId player = nextAssignedPlayer++;
            playerOf[id] = player;
            playerConn[player] = id;
        } else {
            authorityConn = id;
        }
    }

    void LockstepHost::HandleDisconnect(ConnectionId id)
    {
        auto it = playerOf.find(id);
        if (it != playerOf.end()) {
            playerConn.erase(it->second);
            playerOf.erase(it);
        }
        if (id == authorityConn) {
            authorityConn = INVALID_CONNECTION_ID;
        }
    }

    void LockstepHost::HandleInputMessage(ConnectionId id, std::span<const uint8_t> payload)
    {
        ByteReader reader(payload);
        uint64_t frame = 0;
        uint16_t size = 0;
        if (!reader.U64(frame) || !reader.U16(size)) {
            return;
        }
        std::span<const uint8_t> bytes;
        if (!reader.Bytes(size, bytes)) {
            return;
        }
        auto playerIt = playerOf.find(id);
        if (playerIt == playerOf.end()) {
            return;
        }
        const LockstepPlayerId player = playerIt->second;
        auto &slot = collected[frame];
        slot.resize(playerCount);
        auto &present = collectedPresent[frame];
        present.resize(playerCount, false);
        slot[player] = std::vector<uint8_t>(bytes.begin(), bytes.end());
        if (!present[player]) {
            present[player] = true;
            ++collectedCount[frame];
        }
    }

    void LockstepHost::HandleFrameMessage(std::span<const uint8_t> payload)
    {
        ByteReader reader(payload);
        uint64_t frame = 0;
        uint16_t count = 0;
        if (!reader.U64(frame) || !reader.U16(count)) {
            return;
        }
        std::vector<std::vector<uint8_t>> inputs(count);
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t size = 0;
            if (!reader.U16(size)) {
                return;
            }
            std::span<const uint8_t> bytes;
            if (!reader.Bytes(size, bytes)) {
                return;
            }
            inputs[i].assign(bytes.begin(), bytes.end());
        }
        lastKnownInputs = inputs;

        if (frame >= currentFrame) {
            incoming[frame] = inputs;
        } else {
            FrameRecord *record = FindFrame(frame);
            if (record != nullptr && record->inputs != inputs) {
                RollbackTo(frame, inputs);
            }
        }
    }

    void LockstepHost::HandleHashMessage(std::span<const uint8_t> payload)
    {
        ByteReader reader(payload);
        uint64_t frame = 0;
        uint64_t hash = 0;
        if (!reader.U64(frame) || !reader.U64(hash)) {
            return;
        }
        // Buffer the authoritative hash; compared when this frame is applied.
        authoritativeHashes[frame] = hash;
    }

    void LockstepHost::HandleResyncState(std::span<const uint8_t> payload)
    {
        ByteReader reader(payload);
        uint64_t frame = 0;
        uint32_t size = 0;
        if (!reader.U64(frame) || !reader.U32(size)) {
            return;
        }
        std::span<const uint8_t> state;
        if (!reader.Bytes(size, state)) {
            return;
        }
        sim.RestoreState(state);
        currentFrame = frame;
        ring.clear();
        incoming.clear();
        lastStateHash = sim.StateHash();
        ++resyncCount;
    }

    void LockstepHost::BroadcastFrame(LockstepFrame frame, const std::vector<std::vector<uint8_t>> &inputs)
    {
        std::vector<uint8_t> message;
        ByteWriter writer(message);
        writer.U8(static_cast<uint8_t>(LockstepMessage::Frame));
        writer.U64(frame);
        writer.U16(static_cast<uint16_t>(inputs.size()));
        for (const auto &input : inputs) {
            writer.U16(static_cast<uint16_t>(input.size()));
            writer.Bytes(input);
        }
        for (auto &entry : playerConn) {
            host.Send(entry.second, LOCKSTEP_CHANNEL, message, DeliveryMode::ReliableOrdered);
        }
    }

    void LockstepHost::BroadcastHash(LockstepFrame frame, uint64_t hash)
    {
        std::vector<uint8_t> message;
        ByteWriter writer(message);
        writer.U8(static_cast<uint8_t>(LockstepMessage::Hash));
        writer.U64(frame);
        writer.U64(hash);
        for (auto &entry : playerConn) {
            host.Send(entry.second, LOCKSTEP_CHANNEL, message, DeliveryMode::ReliableOrdered);
        }
    }

    void LockstepHost::RequestResync()
    {
        if (!IsValid(authorityConn)) {
            return;
        }
        std::vector<uint8_t> message;
        ByteWriter writer(message);
        writer.U8(static_cast<uint8_t>(LockstepMessage::ResyncRequest));
        host.Send(authorityConn, LOCKSTEP_CHANNEL, message, DeliveryMode::ReliableOrdered);
    }

    void LockstepHost::SendResyncState(ConnectionId id)
    {
        std::vector<uint8_t> state;
        sim.CaptureState(state);
        std::vector<uint8_t> message;
        ByteWriter writer(message);
        writer.U8(static_cast<uint8_t>(LockstepMessage::ResyncState));
        writer.U64(currentFrame);
        writer.U32(static_cast<uint32_t>(state.size()));
        writer.Bytes(state);
        host.Send(id, LOCKSTEP_CHANNEL, message, DeliveryMode::ReliableOrdered);
    }

    void LockstepHost::TrimRing()
    {
        while (ring.size() > static_cast<size_t>(config.rollbackMaxFrames) + 1) {
            ring.pop_front();
        }
    }

    LockstepHost::FrameRecord *LockstepHost::FindFrame(LockstepFrame frame)
    {
        for (auto &record : ring) {
            if (record.frame == frame) {
                return &record;
            }
        }
        return nullptr;
    }

    bool LockstepHost::ReadyToAdvance(LockstepFrame frame) const
    {
        auto it = collectedCount.find(frame);
        return it != collectedCount.end() && it->second >= playerCount;
    }

} // namespace sky::net
