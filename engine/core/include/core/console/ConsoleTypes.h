//
// Created by blues on 2025/5/25.
//

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace sky {

    enum class CVarFlags : uint32_t {
        NONE             = 0,
        READ_ONLY        = 1 << 0,
        CHEAT            = 1 << 1,
        ARCHIVE          = 1 << 2,
        HIDDEN           = 1 << 3,
        REQUIRES_RESTART = 1 << 4,
    };

    inline CVarFlags operator|(CVarFlags a, CVarFlags b) { return static_cast<CVarFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
    inline CVarFlags operator&(CVarFlags a, CVarFlags b) { return static_cast<CVarFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); }
    inline CVarFlags &operator|=(CVarFlags &a, CVarFlags b) { a = a | b; return a; }
    inline bool HasFlag(CVarFlags flags, CVarFlags test) { return (flags & test) != CVarFlags::NONE; }

    using CommandArgs = std::span<const std::string_view>;

    struct CommandResult {
        enum class Status : uint8_t {
            OK,
            ERROR,
            NOT_FOUND,
        };

        Status      status = Status::OK;
        std::string output;
    };

    using CommandFunc = std::function<CommandResult(CommandArgs)>;

} // namespace sky
