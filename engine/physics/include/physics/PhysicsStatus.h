//
// Created on 2026/10/26.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::phy {

    enum class PhysicsError : uint8_t {
        None = 0,
        InvalidArgument,
        Unsupported,
        CapacityExceeded,
        InvalidState,
        BackendFailure
    };

    struct PhysicsStatus {
        PhysicsError error = PhysicsError::None;
        std::string message;

        explicit operator bool() const { return error == PhysicsError::None; }
    };

    struct PhysicsWorldLimits {
        uint32_t bodies = 65536;
        uint32_t characters = 256;
        uint32_t bodyPairs = 65536;
        uint32_t contactConstraints = 16384;
    };

} // namespace sky::phy
