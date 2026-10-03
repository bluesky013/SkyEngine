//
// Created by blues on 2026/10/3.
//

#include "CookWorkerHost.h"

#include <framework/asset/CookProtocol.h>
#include <framework/asset/CookWorker.h>
#include <framework/ipc/FrameChannel.h>
#include <framework/platform/PlatformBase.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

#include <chrono>
#include <thread>
#include <vector>

#if SKY_PLATFORM_WINDOWS
    #include <windows.h>
    #include <io.h>
#else
    #include <cerrno>
    #include <poll.h>
    #include <unistd.h>
#endif

namespace sky {

    namespace {

        constexpr const char *TAG = "CookWorker";
        constexpr uint32_t    POLL_TIMEOUT_MS = 20;

        // Worker-side stdio transport: frames go to a preserved stdout, logs to stderr.
        // The redirected CRT fd 1 (Windows) / fd 1 (POSIX) keeps stray printf off the
        // frame stream; Logger is additionally pointed at stderr by main().
        class WorkerStdio {
        public:
            ~WorkerStdio()
            {
#if !SKY_PLATFORM_WINDOWS
                if (frameFd >= 0) {
                    close(frameFd);
                }
#endif
            }

            WorkerStdio()
            {
#if SKY_PLATFORM_WINDOWS
                frameHandle = GetStdHandle(STD_OUTPUT_HANDLE);
                inputHandle = GetStdHandle(STD_INPUT_HANDLE);
                // Frames go to the OS stdout handle above; redirect the CRT fd so stray
                // printf/iostream write to stderr instead of the frame stream.
                _dup2(2, 1);
#else
                frameFd = dup(STDOUT_FILENO);
                dup2(STDERR_FILENO, STDOUT_FILENO);
#endif
            }

            size_t Read(uint8_t *dst, size_t cap, bool &eof)
            {
                eof = false;
#if SKY_PLATFORM_WINDOWS
                if (inputHandle == nullptr || inputHandle == INVALID_HANDLE_VALUE) {
                    eof = true;
                    return 0;
                }
                DWORD available = 0;
                uint32_t waited = 0;
                for (;;) {
                    if (PeekNamedPipe(inputHandle, nullptr, 0, nullptr, &available, nullptr)) {
                        if (available > 0) {
                            break;
                        }
                    } else {
                        eof = true;
                        return 0;
                    }
                    if (waited >= POLL_TIMEOUT_MS) {
                        return 0;
                    }
                    Sleep(5);
                    waited += 5;
                }
                const DWORD toRead = static_cast<DWORD>(cap < available ? cap : available);
                DWORD read = 0;
                if (!ReadFile(inputHandle, dst, toRead, &read, nullptr)) {
                    eof = true;
                    return 0;
                }
                if (read == 0) {
                    eof = true;
                }
                return read;
#else
                struct pollfd pfd = {};
                pfd.fd = STDIN_FILENO;
                pfd.events = POLLIN;
                const int rc = poll(&pfd, 1, static_cast<int>(POLL_TIMEOUT_MS));
                if (rc == 0) {
                    return 0;
                }
                if (rc < 0) {
                    if (errno == EINTR) {
                        return 0;
                    }
                    eof = true;
                    return 0;
                }
                const ssize_t n = read(STDIN_FILENO, dst, cap);
                if (n < 0) {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                        return 0;
                    }
                    eof = true;
                    return 0;
                }
                if (n == 0) {
                    eof = true;
                    return 0;
                }
                return static_cast<size_t>(n);
#endif
            }

            bool Write(const uint8_t *data, size_t size)
            {
#if SKY_PLATFORM_WINDOWS
                if (frameHandle == nullptr || frameHandle == INVALID_HANDLE_VALUE) {
                    return false;
                }
                size_t written = 0;
                while (written < size) {
                    DWORD chunk = static_cast<DWORD>(size - written > (1u << 20) ? (1u << 20) : (size - written));
                    DWORD done = 0;
                    if (!WriteFile(frameHandle, data + written, chunk, &done, nullptr)) {
                        return false;
                    }
                    written += done;
                }
                return true;
#else
                size_t written = 0;
                while (written < size) {
                    const ssize_t n = write(frameFd, data + written, size - written);
                    if (n < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        return false;
                    }
                    written += static_cast<size_t>(n);
                }
                return true;
#endif
            }

        private:
#if SKY_PLATFORM_WINDOWS
            HANDLE frameHandle = nullptr;
            HANDLE inputHandle = nullptr;
#else
            int frameFd = -1;
#endif
        };

    } // namespace

    CookWorkerHost::CookWorkerHost() = default;

    CookWorkerHost::~CookWorkerHost()
    {
        if (channel) {
            channel->Stop();
        }
    }

    bool CookWorkerHost::Run()
    {
        auto stdio = std::make_unique<WorkerStdio>();

        channel = std::make_unique<FrameChannel>(
            [&stdio](uint8_t *dst, size_t cap, bool &eof) { return stdio->Read(dst, cap, eof); },
            [&stdio](const uint8_t *data, size_t size) { return stdio->Write(data, size); });
        channel->SetFrameHandler([this](const uint8_t *data, size_t size) { HandleFrame(data, size); });
        channel->SetErrorHandler([this](const char *reason) {
            LOG_E(TAG, "channel error: %s", reason);
            quit.store(true);
        });
        channel->Start();

        // Also exit when the reader stops (parent disconnected/closed the pipe), so a dead
        // host can never leave the worker hanging.
        while (!quit.load() && channel->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        const bool ok = !channel->IsFailed();
        channel->Stop();
        channel.reset();
        stdio.reset();
        return ok;
    }

    void CookWorkerHost::HandleFrame(const uint8_t *data, size_t size)
    {
        CookMessage message;
        if (!DecodeCookMessage(std::string(reinterpret_cast<const char *>(data), size), message)) {
            LOG_W(TAG, "dropping malformed frame");
            return;
        }

        switch (message.type) {
            case CookMessageType::Hello:
                if (message.protocol != COOK_PROTOCOL_VERSION) {
                    LOG_E(TAG, "protocol mismatch: parent %u, worker %u", message.protocol, COOK_PROTOCOL_VERSION);
                    quit.store(true);
                } else {
                    SendReady();
                }
                break;
            case CookMessageType::Cook: {
                CookWorker worker;
                worker.CookBatch({{message.uuid, message.target}});
                SendResult(message.id, message.uuid, message.target, 0, {});
                break;
            }
            case CookMessageType::Ping: {
                CookMessage pong = {};
                pong.type = CookMessageType::Pong;
                channel->WriteFrame(EncodeCookMessage(pong));
                break;
            }
            case CookMessageType::Shutdown:
                quit.store(true);
                break;
            default:
                break;
        }
    }

    void CookWorkerHost::SendReady()
    {
        CookMessage ready = {};
        ready.type = CookMessageType::Ready;
        ready.protocol = COOK_PROTOCOL_VERSION;
        ready.platform = Platform::GetPlatformNameByType(Platform::Get()->GetType());
        channel->WriteFrame(EncodeCookMessage(ready));
    }

    void CookWorkerHost::SendResult(uint64_t id, const Uuid &uuid, const std::string &target, int32_t retCode, const std::string &error)
    {
        CookMessage result = {};
        result.type = CookMessageType::Result;
        result.id = id;
        result.uuid = uuid;
        result.target = target;
        result.retCode = retCode;
        result.error = error;
        channel->WriteFrame(EncodeCookMessage(result));
    }

} // namespace sky
