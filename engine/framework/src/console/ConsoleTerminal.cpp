//
// Created by blues on 2025/5/25.
//

#include <framework/console/ConsoleTerminal.h>
#include <core/console/CommandRegistry.h>
#include <core/console/CVar.h>
#include <sstream>
#include <algorithm>

namespace sky {

    static const char *PROMPT = "> ";
    static const size_t PROMPT_LEN = 2;

    ConsoleTerminal::ConsoleTerminal(ITerminalIO *io)
        : io_(io)
        , tty_(io ? io->IsTTY() : false)
    {
    }

    void ConsoleTerminal::DrawPrompt()
    {
        if (tty_) {
            io_->Write(PROMPT);
        }
    }

    bool ConsoleTerminal::PollInput(std::string &outLine)
    {
        if (!io_) return false;

        if (!tty_) {
            return ReadLine(outLine);
        }

        hasLine_ = false;
        char ch;
        while (io_->PollChar(ch)) {
            HandleChar(ch);
            if (hasLine_) {
                outLine = std::move(buffer_);
                buffer_.clear();
                cursor_ = 0;
                return true;
            }
        }
        return false;
    }

    bool ConsoleTerminal::ReadLine(std::string &outLine)
    {
        if (!io_) return false;

        char ch;
        while (io_->PollChar(ch)) {
            if (ch == '\n' || ch == '\r') {
                outLine = std::move(pipeBuffer_);
                pipeBuffer_.clear();
                return true;
            }
            pipeBuffer_ += ch;
        }
        return false;
    }

    void ConsoleTerminal::HandleChar(char ch)
    {
        switch (escState_) {
        case EscState::NONE:
            if (ch == '\033') {
                escState_ = EscState::ESC;
                escBuf_.clear();
            } else if (ch == '\r' || ch == '\n') {
                io_->Write("\r\n");
                hasLine_ = true;
            } else if (ch == '\t') {
                HandleTab();
            } else if (ch == 127 || ch == '\b') {
                DeleteCharBefore();
            } else if (ch >= 32) {
                InsertChar(ch);
            } else if (ch == 1) { // Ctrl+A -> Home
                MoveCursorHome();
            } else if (ch == 5) { // Ctrl+E -> End
                MoveCursorEnd();
            }
            break;

        case EscState::ESC:
            if (ch == '[') {
                escState_ = EscState::BRACKET;
            } else {
                escState_ = EscState::NONE;
            }
            break;

        case EscState::BRACKET:
            escBuf_ += ch;
            if (ch >= 'A' && ch <= 'Z') {
                HandleEscape();
                escState_ = EscState::NONE;
            } else if (ch == '~') {
                HandleEscape();
                escState_ = EscState::NONE;
            } else if (ch >= '0' && ch <= '9') {
                // accumulate numeric parameter
            } else {
                escState_ = EscState::NONE;
            }
            break;
        }
    }

    void ConsoleTerminal::HandleEscape()
    {
        if (escBuf_ == "A") {
            // Up arrow -- history previous
            if (history_) {
                auto prev = history_->GetPrevious();
                if (!prev.empty()) {
                    buffer_ = std::string(prev);
                    cursor_ = buffer_.size();
                    RedrawLine();
                }
            }
        } else if (escBuf_ == "B") {
            // Down arrow -- history next
            if (history_) {
                auto next = history_->GetNext();
                buffer_ = std::string(next);
                cursor_ = buffer_.size();
                RedrawLine();
            }
        } else if (escBuf_ == "C") {
            MoveCursorRight();
        } else if (escBuf_ == "D") {
            MoveCursorLeft();
        } else if (escBuf_ == "H") {
            MoveCursorHome();
        } else if (escBuf_ == "F") {
            MoveCursorEnd();
        } else if (escBuf_ == "3~") {
            DeleteCharAt();
        }
    }

    void ConsoleTerminal::HandleTab()
    {
        auto *registry = CommandRegistry::Get();
        auto matches = registry->FindByPrefix(buffer_);
        if (matches.empty()) {
            return;
        }

        // print matches below current line
        io_->Write("\r\n");

        for (const auto &match : matches) {
            if (match.cvar) {
                std::string line = "  " + std::string(match.cvar->GetName())
                    + " (" + std::string(match.cvar->GetTypeName()) + ") = " + match.cvar->ToString() + "\n";
                io_->Write(line);
            } else if (match.cmd) {
                std::string line = "  " + match.cmd->name + " -- " + match.cmd->desc + "\n";
                io_->Write(line);
            }
        }

        // redraw prompt + buffer
        RedrawLine();
    }

    void ConsoleTerminal::InsertChar(char ch)
    {
        buffer_.insert(buffer_.begin() + static_cast<ptrdiff_t>(cursor_), ch);
        ++cursor_;

        if (cursor_ == buffer_.size()) {
            // append at end -- just echo the char
            char str[2] = { ch, 0 };
            io_->Write(std::string_view(str, 1));
        } else {
            // mid-insert: redraw from cursor
            RedrawLine();
        }
    }

    void ConsoleTerminal::DeleteCharBefore()
    {
        if (cursor_ == 0) return;
        buffer_.erase(cursor_ - 1, 1);
        --cursor_;
        RedrawLine();
    }

    void ConsoleTerminal::DeleteCharAt()
    {
        if (cursor_ >= buffer_.size()) return;
        buffer_.erase(cursor_, 1);
        RedrawLine();
    }

    void ConsoleTerminal::MoveCursorLeft()
    {
        if (cursor_ > 0) {
            --cursor_;
            io_->Write("\033[D");
        }
    }

    void ConsoleTerminal::MoveCursorRight()
    {
        if (cursor_ < buffer_.size()) {
            ++cursor_;
            io_->Write("\033[C");
        }
    }

    void ConsoleTerminal::MoveCursorHome()
    {
        if (cursor_ > 0) {
            cursor_ = 0;
            RedrawLine();
        }
    }

    void ConsoleTerminal::MoveCursorEnd()
    {
        if (cursor_ < buffer_.size()) {
            cursor_ = buffer_.size();
            RedrawLine();
        }
    }

    void ConsoleTerminal::EraseLine()
    {
        io_->Write("\r\033[K");
    }

    void ConsoleTerminal::RedrawLine()
    {
        EraseLine();
        io_->Write(PROMPT);
        io_->Write(buffer_);

        // move cursor to correct position
        if (cursor_ < buffer_.size()) {
            size_t back = buffer_.size() - cursor_;
            std::string moveBack = "\033[" + std::to_string(back) + "D";
            io_->Write(moveBack);
        }
    }

    void ConsoleTerminal::WriteLogEntry(const LogEntry &entry)
    {
        if (!tty_) {
            // no ANSI, just plain text
            std::string line = "[" + entry.level + " " + entry.tag + "] " + entry.message + "\n";
            io_->Write(line);
            return;
        }

        // erase current input line
        EraseLine();

        // format with ANSI colors
        std::string line;
        if (entry.level == "ERROR") {
            line = "\033[31m[" + entry.level + " " + entry.tag + "] " + entry.message + "\033[0m\n";
        } else if (entry.level == "WARNING") {
            line = "\033[33m[" + entry.level + " " + entry.tag + "] " + entry.message + "\033[0m\n";
        } else {
            line = "[" + entry.level + " " + entry.tag + "] " + entry.message + "\n";
        }
        io_->Write(line);

        // redraw prompt + buffer
        io_->Write(PROMPT);
        io_->Write(buffer_);

        // reposition cursor
        if (cursor_ < buffer_.size()) {
            size_t back = buffer_.size() - cursor_;
            std::string moveBack = "\033[" + std::to_string(back) + "D";
            io_->Write(moveBack);
        }
    }

} // namespace sky
