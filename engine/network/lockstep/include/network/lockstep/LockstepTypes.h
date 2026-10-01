//
// Created on 2026/10/01.
//

#pragma once

#include <cstdint>

namespace sky::net {

    using LockstepFrame    = uint64_t;
    using LockstepPlayerId = uint32_t;

    // Reserved reliable-ordered channel for lockstep control and input frames.
    inline constexpr uint8_t LOCKSTEP_CHANNEL = 3;

    enum class LockstepMessage : uint8_t {
        Input        = 1,   // client -> authority: one player's input for a frame
        Frame        = 2,   // authority -> clients: the authoritative input frame
        Hash         = 3,   // authority -> clients: authoritative state hash for a frame
        ResyncRequest = 4,  // client -> authority: request a state snapshot
        ResyncState  = 5    // authority -> client: state snapshot to restore
    };

} // namespace sky::net
