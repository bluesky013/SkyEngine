//
// Created by blues on 2025/5/25.
//

#pragma once

#include <string_view>

namespace sky {

    class ITerminalIO {
    public:
        ITerminalIO() = default;
        virtual ~ITerminalIO() = default;

        static ITerminalIO *Create();

        virtual bool PollChar(char &ch) = 0;
        virtual void Write(std::string_view text) = 0;
        virtual int  GetWidth() = 0;
        virtual void EnableRawMode() = 0;
        virtual void RestoreMode() = 0;
        virtual bool IsTTY() = 0;
    };

} // namespace sky
