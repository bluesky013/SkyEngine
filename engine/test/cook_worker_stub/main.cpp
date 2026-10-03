//
// Protocol-speaking stub worker for OutOfProcessCookRunnerTest. Speaks the cook frame
// protocol on stdin/stdout synchronously so the runner can be exercised without builders.
//
// Modes (argv[1]):
//   ok          handshake, reply to every cook, write logs to stderr
//   timeout     handshake, never reply to a cook
//   crash       handshake, exit(3) on the first cook
//   crashonce   exit(3) on the first cook, then behave like ok
//   spurious    handshake, emit a result with an unknown id, then behave like ok
//

#include <framework/asset/CookProtocol.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#if defined(_WIN32)
    #include <io.h>
    #include <process.h>
static int ReadFd(int fd, void *buf, unsigned int size) { return _read(fd, buf, size); }
static int WriteFd(int fd, const void *buf, unsigned int size) { return _write(fd, buf, size); }
static void ExitNow(int code) { _exit(code); }
#else
    #include <unistd.h>
static int ReadFd(int fd, void *buf, unsigned int size) { return static_cast<int>(read(fd, buf, size)); }
static int WriteFd(int fd, const void *buf, unsigned int size) { return static_cast<int>(write(fd, buf, size)); }
static void ExitNow(int code) { _exit(code); }
#endif

using namespace sky;

namespace {

    bool ReadExact(void *buffer, size_t size)
    {
        size_t got = 0;
        while (got < size) {
            const int count = ReadFd(0, static_cast<char *>(buffer) + got, static_cast<unsigned int>(size - got));
            if (count <= 0) {
                return false;
            }
            got += static_cast<size_t>(count);
        }
        return true;
    }

    bool WriteAll(const void *buffer, size_t size)
    {
        size_t written = 0;
        while (written < size) {
            const int count = WriteFd(1, static_cast<const char *>(buffer) + written, static_cast<unsigned int>(size - written));
            if (count <= 0) {
                return false;
            }
            written += static_cast<size_t>(count);
        }
        return true;
    }

    bool ReadFrame(CookMessage &message)
    {
        uint8_t header[4];
        if (!ReadExact(header, 4)) {
            return false;
        }
        const uint32_t length = static_cast<uint32_t>(header[0]) | (static_cast<uint32_t>(header[1]) << 8) |
                                (static_cast<uint32_t>(header[2]) << 16) | (static_cast<uint32_t>(header[3]) << 24);
        std::string payload(length, '\0');
        if (length > 0 && !ReadExact(payload.data(), length)) {
            return false;
        }
        return DecodeCookMessage(payload, message);
    }

    bool SentinelExists(const char *path)
    {
        if (path == nullptr) {
            return false;
        }
        std::ifstream file(path);
        return file.good();
    }

    void CreateSentinel(const char *path)
    {
        if (path == nullptr) {
            return;
        }
        std::ofstream file(path);
    }

    bool WriteFrame(const CookMessage &message)
    {
        const std::string payload = EncodeCookMessage(message);
        const uint32_t length = static_cast<uint32_t>(payload.size());
        const uint8_t header[4] = {
            static_cast<uint8_t>(length & 0xff),
            static_cast<uint8_t>((length >> 8) & 0xff),
            static_cast<uint8_t>((length >> 16) & 0xff),
            static_cast<uint8_t>((length >> 24) & 0xff)};
        return WriteAll(header, 4) && (length == 0 || WriteAll(payload.data(), length));
    }

} // namespace

int main(int argc, char **argv)
{
    const std::string mode = (argc > 1) ? argv[1] : "ok";

    bool ready = false;
    int  cookCount = 0;

    CookMessage message;
    while (ReadFrame(message)) {
        switch (message.type) {
            case CookMessageType::Hello: {
                CookMessage response = {};
                response.type = CookMessageType::Ready;
                response.protocol = COOK_PROTOCOL_VERSION;
                response.platform = "Test";
                WriteFrame(response);
                ready = true;

                if (mode == "spurious") {
                    CookMessage bogus = {};
                    bogus.type = CookMessageType::Result;
                    bogus.id = 9999;
                    bogus.uuid = Uuid::Create();
                    bogus.target = "common";
                    WriteFrame(bogus);
                }
                break;
            }
            case CookMessageType::Cook: {
                ++cookCount;

                if (mode == "crash") {
                    ExitNow(3);
                }
                if (mode == "crashonce" && cookCount == 1) {
                    // Crash once; a restarted process (sentinel present) then behaves normally.
                    const char *sentinel = (argc > 2) ? argv[2] : nullptr;
                    if (!SentinelExists(sentinel)) {
                        CreateSentinel(sentinel);
                        ExitNow(3);
                    }
                }
                if (mode == "timeout") {
                    break;
                }

                std::fprintf(stderr, "stub: cooking id=%llu\n", static_cast<unsigned long long>(message.id));
                std::fflush(stderr);

                CookMessage result = {};
                result.type = CookMessageType::Result;
                result.id = message.id;
                result.uuid = message.uuid;
                result.target = message.target;
                result.retCode = 0;
                WriteFrame(result);

                std::fprintf(stderr, "stub: done id=%llu\n", static_cast<unsigned long long>(message.id));
                std::fflush(stderr);
                break;
            }
            case CookMessageType::Ping: {
                CookMessage pong = {};
                pong.type = CookMessageType::Pong;
                WriteFrame(pong);
                break;
            }
            case CookMessageType::Shutdown:
                return 0;
            default:
                break;
        }
    }

    (void)ready;
    return 0;
}
