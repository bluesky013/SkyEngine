//
// Created by blues on 2026/10/3.
//

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace sky {

    // Length-prefixed frame channel over a byte transport: the frame stream carries
    // ([u32 LE length][payload bytes]), the optional log stream carries text lines.
    // Payloads are opaque bytes; JSON is layered on top by the cook protocol.
    //
    // Reads must block for at most a short interval and return 0 with eof=false when no
    // data is available, so the reader thread can observe Stop() without a cancel API.
    class FrameChannel {
    public:
        using ReadFn = std::function<size_t(uint8_t *dst, size_t cap, bool &eof)>;
        using WriteFn = std::function<bool(const uint8_t *data, size_t size)>;
        using FrameHandler = std::function<void(const uint8_t *data, size_t size)>;
        using LogHandler = std::function<void(const std::string &line)>;
        using ErrorHandler = std::function<void(const char *reason)>;

        FrameChannel(ReadFn reader, WriteFn writer, ReadFn logReader = {});
        ~FrameChannel();

        FrameChannel(const FrameChannel &) = delete;
        FrameChannel &operator=(const FrameChannel &) = delete;

        void SetFrameHandler(FrameHandler handler);
        void SetLogHandler(LogHandler handler);
        void SetErrorHandler(ErrorHandler handler);
        // Set before Start() (read on the reader thread without synchronization).
        void SetMaxFrameBytes(size_t maxFrameBytes);

        bool WriteFrame(const uint8_t *data, size_t size);
        bool WriteFrame(const std::string &payload);

        void Start();
        // Stops and joins the reader/log threads. Reads poll with a short timeout,
        // so Stop returns promptly without closing handles under a blocked read.
        void Stop();

        bool IsRunning() const { return running.load(); }
        bool IsFailed() const { return failed.load(); }

    private:
        void ReaderLoop();
        void LogLoop();
        void Fail(const char *reason);
        void DispatchFrame(const uint8_t *data, size_t size);
        void DispatchLog(const std::string &line);

        ReadFn  reader;
        WriteFn writer;
        ReadFn  logReader;
        size_t  maxFrame = 16u * 1024u * 1024u;

        FrameHandler frameHandler;
        LogHandler   logHandler;
        ErrorHandler errorHandler;

        std::thread       readerThread;
        std::thread       logThread;
        std::atomic<bool> running{false};
        std::atomic<bool> failed{false};

        std::mutex writeMutex;
        std::mutex handlerMutex;
    };

} // namespace sky
