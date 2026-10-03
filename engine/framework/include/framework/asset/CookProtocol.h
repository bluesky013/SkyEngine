//
// Created by blues on 2026/10/3.
//

#pragma once

#include <cstdint>
#include <string>
#include <core/util/Uuid.h>

namespace sky {

    inline constexpr uint32_t COOK_PROTOCOL_VERSION = 1;

    enum class CookMessageType : uint32_t {
        Unknown = 0,
        Hello,
        Ready,
        Cook,
        Result,
        Ping,
        Pong,
        Shutdown,
    };

    struct CookMessage {
        CookMessageType type = CookMessageType::Unknown;
        uint32_t        protocol = COOK_PROTOCOL_VERSION;
        uint64_t        id = 0;
        Uuid            uuid;
        std::string     target;
        std::string     path;
        int32_t         retCode = 0;
        std::string     error;
        std::string     platform;
    };

    std::string EncodeCookMessage(const CookMessage &message);
    bool DecodeCookMessage(const std::string &payload, CookMessage &out);
    const char *GetCookMessageTypeName(CookMessageType type);

} // namespace sky
