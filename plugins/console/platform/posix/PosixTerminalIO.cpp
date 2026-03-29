//
// Created by blues on 2025/5/25.
//

#include <framework/console/ITerminalIO.h>

#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <cstdio>

namespace sky {

    class PosixTerminalIO : public ITerminalIO {
    public:
        PosixTerminalIO() = default;
        ~PosixTerminalIO() override = default;

        bool PollChar(char &ch) override
        {
            if (!rawMode_) return false;

            struct pollfd pfd;
            pfd.fd = STDIN_FILENO;
            pfd.events = POLLIN;
            if (poll(&pfd, 1, 0) > 0) {
                if (read(STDIN_FILENO, &ch, 1) == 1) {
                    return true;
                }
            }
            return false;
        }

        void Write(std::string_view text) override
        {
            ::write(STDOUT_FILENO, text.data(), text.size());
        }

        int GetWidth() override
        {
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
                return ws.ws_col;
            }
            return 80;
        }

        void EnableRawMode() override
        {
            if (rawMode_) return;
            if (!IsTTY()) return;

            tcgetattr(STDIN_FILENO, &origTermios_);
            struct termios raw = origTermios_;
            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            raw.c_oflag &= ~(OPOST);
            raw.c_cflag |= (CS8);
            raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            rawMode_ = true;
        }

        void RestoreMode() override
        {
            if (!rawMode_) return;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios_);
            rawMode_ = false;
        }

        bool IsTTY() override
        {
            return isatty(STDIN_FILENO) != 0;
        }

    private:
        struct termios origTermios_ = {};
        bool rawMode_ = false;
    };

    ITerminalIO *ITerminalIO::Create()
    {
        return new PosixTerminalIO();
    }

} // namespace sky
