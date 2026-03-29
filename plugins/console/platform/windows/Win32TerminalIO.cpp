//
// Created by blues on 2025/5/25.
//

#include <framework/console/ITerminalIO.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>
#include <cstdio>

namespace sky {

    class Win32TerminalIO : public ITerminalIO {
    public:
        Win32TerminalIO()
        {
            hStdin_  = GetStdHandle(STD_INPUT_HANDLE);
            hStdout_ = GetStdHandle(STD_OUTPUT_HANDLE);

            // When launched under a debugger (e.g. VS), stdin/stdout may be
            // redirected so _isatty() returns false even though a console
            // window exists.  Open CONIN$/CONOUT$ directly to get real
            // console handles and mark that we forced them.
            if (_isatty(_fileno(stdin)) == 0 && GetConsoleWindow() != nullptr) {
                HANDLE hIn  = CreateFileA("CONIN$",  GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                HANDLE hOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                if (hIn != INVALID_HANDLE_VALUE && hOut != INVALID_HANDLE_VALUE) {
                    hStdin_  = hIn;
                    hStdout_ = hOut;
                    forcedConsole_ = true;
                }
            }
        }

        ~Win32TerminalIO() override
        {
            if (forcedConsole_) {
                CloseHandle(hStdin_);
                CloseHandle(hStdout_);
            }
        }

        bool PollChar(char &ch) override
        {
            if (!rawMode_) {
                return false;
            }

            // drain escape buffer first
            if (escPos_ < escLen_) {
                ch = escBuf_[escPos_++];
                if (escPos_ >= escLen_) {
                    escLen_ = 0;
                    escPos_ = 0;
                }
                return true;
            }

            DWORD count = 0;
            INPUT_RECORD record;
            while (PeekConsoleInputA(hStdin_, &record, 1, &count) && count > 0) {
                ReadConsoleInputA(hStdin_, &record, 1, &count);
                if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
                    char c = record.Event.KeyEvent.uChar.AsciiChar;
                    WORD vk = record.Event.KeyEvent.wVirtualKeyCode;

                    if (vk == VK_UP)         { escBuf_[0] = '\033'; escBuf_[1] = '['; escBuf_[2] = 'A'; escLen_ = 3; escPos_ = 0; }
                    else if (vk == VK_DOWN)  { escBuf_[0] = '\033'; escBuf_[1] = '['; escBuf_[2] = 'B'; escLen_ = 3; escPos_ = 0; }
                    else if (vk == VK_RIGHT) { escBuf_[0] = '\033'; escBuf_[1] = '['; escBuf_[2] = 'C'; escLen_ = 3; escPos_ = 0; }
                    else if (vk == VK_LEFT)  { escBuf_[0] = '\033'; escBuf_[1] = '['; escBuf_[2] = 'D'; escLen_ = 3; escPos_ = 0; }
                    else if (vk == VK_HOME)  { escBuf_[0] = '\033'; escBuf_[1] = '['; escBuf_[2] = 'H'; escLen_ = 3; escPos_ = 0; }
                    else if (vk == VK_END)   { escBuf_[0] = '\033'; escBuf_[1] = '['; escBuf_[2] = 'F'; escLen_ = 3; escPos_ = 0; }
                    else if (vk == VK_DELETE) { escBuf_[0] = '\033'; escBuf_[1] = '['; escBuf_[2] = '3'; escBuf_[3] = '~'; escLen_ = 4; escPos_ = 0; }
                    else if (c != 0) {
                        ch = c;
                        return true;
                    } else {
                        continue;
                    }

                    if (escLen_ > 0) {
                        ch = escBuf_[escPos_++];
                        if (escPos_ >= escLen_) {
                            escLen_ = 0;
                            escPos_ = 0;
                        }
                        return true;
                    }
                }
            }

            return false;
        }

        void Write(std::string_view text) override
        {
            DWORD written = 0;
            WriteConsoleA(hStdout_, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        }

        int GetWidth() override
        {
            CONSOLE_SCREEN_BUFFER_INFO info;
            if (GetConsoleScreenBufferInfo(hStdout_, &info)) {
                return info.srWindow.Right - info.srWindow.Left + 1;
            }
            return 80;
        }

        void EnableRawMode() override
        {
            if (rawMode_) return;
            if (!IsTTY()) return;

            GetConsoleMode(hStdin_, &origInputMode_);
            GetConsoleMode(hStdout_, &origOutputMode_);

            DWORD inputMode = origInputMode_;
            inputMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
            inputMode |= ENABLE_WINDOW_INPUT;
            SetConsoleMode(hStdin_, inputMode);

            DWORD outputMode = origOutputMode_;
            outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            vtEnabled_ = SetConsoleMode(hStdout_, outputMode) != 0;

            rawMode_ = true;
        }

        void RestoreMode() override
        {
            if (!rawMode_) return;
            SetConsoleMode(hStdin_, origInputMode_);
            SetConsoleMode(hStdout_, origOutputMode_);
            rawMode_ = false;
        }

        bool IsTTY() override
        {
            // If we forced console handles via CONIN$/CONOUT$, treat as TTY
            return forcedConsole_ || _isatty(_fileno(stdin)) != 0;
        }

    private:
        HANDLE hStdin_  = INVALID_HANDLE_VALUE;
        HANDLE hStdout_ = INVALID_HANDLE_VALUE;
        DWORD  origInputMode_  = 0;
        DWORD  origOutputMode_ = 0;
        bool   rawMode_   = false;
        bool   vtEnabled_ = false;
        bool   forcedConsole_ = false;
        char   escBuf_[8] = {};
        int    escPos_ = 0;
        int    escLen_ = 0;
    };

    ITerminalIO *ITerminalIO::Create()
    {
        return new Win32TerminalIO();
    }

} // namespace sky
