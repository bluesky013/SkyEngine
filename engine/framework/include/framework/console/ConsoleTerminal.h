//
// Created by blues on 2025/5/25.
//

#pragma once

#include <framework/console/ITerminalIO.h>
#include <framework/console/CommandHistory.h>
#include <framework/console/ConsoleLog.h>
#include <functional>
#include <memory>
#include <string>

namespace sky {

    class CommandRegistry;

    class ConsoleTerminal {
    public:
        explicit ConsoleTerminal(ITerminalIO *io);
        ~ConsoleTerminal() = default;

        // called every tick -- returns a completed line or empty
        bool PollInput(std::string &outLine);

        void WriteLogEntry(const LogEntry &entry);
        void SetHistory(CommandHistory *history) { history_ = history; }

        // for non-TTY (pipe) mode: read a full line
        bool ReadLine(std::string &outLine);

        bool IsTTY() const { return tty_; }

        void DrawPrompt();

    private:
        void HandleChar(char ch);
        void HandleEscape();
        void HandleTab();

        void InsertChar(char ch);
        void DeleteCharBefore();
        void DeleteCharAt();
        void MoveCursorLeft();
        void MoveCursorRight();
        void MoveCursorHome();
        void MoveCursorEnd();
        void Submit(std::string &outLine);

        void RedrawLine();
        void EraseLine();

        ITerminalIO    *io_;
        CommandHistory *history_ = nullptr;
        bool            tty_ = false;

        std::string     buffer_;
        size_t          cursor_ = 0;
        bool            hasLine_ = false;

        // escape sequence state machine
        enum class EscState { NONE, ESC, BRACKET };
        EscState escState_ = EscState::NONE;
        std::string escBuf_;

        // pipe mode line buffer
        std::string pipeBuffer_;
    };

} // namespace sky

