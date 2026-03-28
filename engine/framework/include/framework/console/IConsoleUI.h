//
// Created by Copilot on 2026/3/29.
//

#pragma once

#include <string>

namespace sky {

    struct LogEntry;

    class IConsoleUI {
    public:
        IConsoleUI()          = default;
        virtual ~IConsoleUI() = default;

        virtual void Toggle()                        = 0;
        virtual void Show()                          = 0;
        virtual void Hide()                          = 0;
        virtual bool IsVisible() const               = 0;
        virtual bool WantsInput() const              = 0;

        virtual void PushLog(const LogEntry &entry)  = 0;
        virtual bool PollCommand(std::string &outLine) = 0;
        virtual void PushOutput(const std::string &text) = 0;
    };

} // namespace sky
