//
// Created by blues on 2026/10/3.
//

#include <framework/ipc/FrameChannel.h>

#include <utility>
#include <vector>

namespace sky {

    namespace {
        constexpr size_t HEADER_SIZE = 4;

        uint32_t DecodeLength(const uint8_t *data)
        {
            return static_cast<uint32_t>(data[0]) |
                   (static_cast<uint32_t>(data[1]) << 8) |
                   (static_cast<uint32_t>(data[2]) << 16) |
                   (static_cast<uint32_t>(data[3]) << 24);
        }

        void EncodeLength(uint8_t *out, size_t size)
        {
            out[0] = static_cast<uint8_t>(size & 0xff);
            out[1] = static_cast<uint8_t>((size >> 8) & 0xff);
            out[2] = static_cast<uint8_t>((size >> 16) & 0xff);
            out[3] = static_cast<uint8_t>((size >> 24) & 0xff);
        }
    } // namespace

    FrameChannel::FrameChannel(ReadFn inReader, WriteFn inWriter, ReadFn inLogReader)
        : reader(std::move(inReader)), writer(std::move(inWriter)), logReader(std::move(inLogReader))
    {
    }

    FrameChannel::~FrameChannel()
    {
        Stop();
    }

    void FrameChannel::SetFrameHandler(FrameHandler handler)
    {
        std::lock_guard<std::mutex> lock(handlerMutex);
        frameHandler = std::move(handler);
    }

    void FrameChannel::SetLogHandler(LogHandler handler)
    {
        std::lock_guard<std::mutex> lock(handlerMutex);
        logHandler = std::move(handler);
    }

    void FrameChannel::SetErrorHandler(ErrorHandler handler)
    {
        std::lock_guard<std::mutex> lock(handlerMutex);
        errorHandler = std::move(handler);
    }

    void FrameChannel::SetMaxFrameBytes(size_t maxFrameBytes)
    {
        maxFrame = maxFrameBytes;
    }

    bool FrameChannel::WriteFrame(const uint8_t *data, size_t size)
    {
        if (!writer) {
            return false;
        }
        std::lock_guard<std::mutex> lock(writeMutex);

        uint8_t header[HEADER_SIZE];
        EncodeLength(header, size);
        if (!writer(header, HEADER_SIZE)) {
            return false;
        }
        if (size > 0 && !writer(data, size)) {
            return false;
        }
        return true;
    }

    bool FrameChannel::WriteFrame(const std::string &payload)
    {
        return WriteFrame(reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
    }

    void FrameChannel::Start()
    {
        if (running.exchange(true)) {
            return;
        }
        failed.store(false);
        if (reader) {
            readerThread = std::thread([this]() { ReaderLoop(); });
        }
        if (logReader) {
            logThread = std::thread([this]() { LogLoop(); });
        }
    }

    void FrameChannel::Stop()
    {
        running.store(false);
        if (readerThread.joinable()) {
            readerThread.join();
        }
        if (logThread.joinable()) {
            logThread.join();
        }
    }

    void FrameChannel::ReaderLoop()
    {
        std::vector<uint8_t> buffer;
        buffer.reserve(64 * 1024);

        while (running.load()) {
            uint8_t chunk[16 * 1024];
            bool    eof = false;
            const size_t count = reader(chunk, sizeof(chunk), eof);

            if (count > 0) {
                buffer.insert(buffer.end(), chunk, chunk + count);
            }

            for (;;) {
                if (buffer.size() < HEADER_SIZE) {
                    break;
                }
                const uint32_t length = DecodeLength(buffer.data());
                if (length > maxFrame) {
                    Fail("frame exceeds maximum size");
                    return;
                }
                if (buffer.size() < HEADER_SIZE + length) {
                    break;
                }
                DispatchFrame(buffer.data() + HEADER_SIZE, length);
                buffer.erase(buffer.begin(), buffer.begin() + HEADER_SIZE + length);
            }

            if (eof) {
                if (!buffer.empty() && running.load()) {
                    Fail("truncated frame at end of stream");
                }
                return;
            }
        }
    }

    void FrameChannel::LogLoop()
    {
        std::string pending;
        while (running.load()) {
            uint8_t chunk[4096];
            bool    eof = false;
            const size_t count = logReader(chunk, sizeof(chunk), eof);

            if (count > 0) {
                pending.append(reinterpret_cast<const char *>(chunk), count);
                size_t pos = 0;
                while ((pos = pending.find('\n')) != std::string::npos) {
                    DispatchLog(pending.substr(0, pos));
                    pending.erase(0, pos + 1);
                }
            }

            if (eof) {
                if (!pending.empty()) {
                    DispatchLog(pending);
                }
                return;
            }
        }
    }

    void FrameChannel::Fail(const char *reason)
    {
        failed.store(true);
        running.store(false);

        ErrorHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlerMutex);
            handler = errorHandler;
        }
        if (handler) {
            handler(reason);
        }
    }

    void FrameChannel::DispatchFrame(const uint8_t *data, size_t size)
    {
        FrameHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlerMutex);
            handler = frameHandler;
        }
        if (handler) {
            handler(data, size);
        }
    }

    void FrameChannel::DispatchLog(const std::string &line)
    {
        LogHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlerMutex);
            handler = logHandler;
        }
        if (handler) {
            handler(line);
        }
    }

} // namespace sky
