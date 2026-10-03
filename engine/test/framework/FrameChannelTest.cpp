//
// Created by blues on 2026/10/3.
//

#include <gtest/gtest.h>

#include <framework/ipc/FrameChannel.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sky;

namespace {

    // Returns one byte per read to exercise partial-frame reassembly; logs delivered as a block.
    struct FakeTransport {
        std::vector<uint8_t> in;
        std::vector<uint8_t> out;
        std::vector<uint8_t> logIn;
        size_t               pos = 0;
        size_t               logPos = 0;

        size_t Read(uint8_t *dst, size_t, bool &eof)
        {
            if (pos >= in.size()) {
                eof = true;
                return 0;
            }
            dst[0] = in[pos++];
            return 1;
        }

        size_t ReadLog(uint8_t *dst, size_t cap, bool &eof)
        {
            if (logPos >= logIn.size()) {
                eof = true;
                return 0;
            }
            const size_t count = std::min(cap, logIn.size() - logPos);
            std::memcpy(dst, logIn.data() + logPos, count);
            logPos += count;
            return count;
        }

        bool Write(const uint8_t *data, size_t size)
        {
            out.insert(out.end(), data, data + size);
            return true;
        }
    };

    std::unique_ptr<FrameChannel> MakeChannel(FakeTransport &transport, size_t maxFrame = 16u * 1024u * 1024u)
    {
        auto channel = std::make_unique<FrameChannel>(
            [&transport](uint8_t *dst, size_t cap, bool &eof) { return transport.Read(dst, cap, eof); },
            [&transport](const uint8_t *data, size_t size) { return transport.Write(data, size); },
            [&transport](uint8_t *dst, size_t cap, bool &eof) { return transport.ReadLog(dst, cap, eof); });
        channel->SetMaxFrameBytes(maxFrame);
        return channel;
    }

    bool WaitFor(const std::atomic<bool> &flag, int timeoutMs = 3000)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!flag.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return flag.load();
    }

} // namespace

TEST(FrameChannelTest, WriteFrameEncodesLengthPrefix)
{
    FakeTransport transport;
    auto channel = MakeChannel(transport);

    ASSERT_TRUE(channel->WriteFrame(std::string("hi")));

    const std::vector<uint8_t> expected = {2, 0, 0, 0, 'h', 'i'};
    EXPECT_EQ(transport.out, expected);
}

TEST(FrameChannelTest, ReassemblesFrameAcrossPartialReads)
{
    FakeTransport transport;
    transport.in = {3, 0, 0, 0, 'a', 'b', 'c'};

    auto channel = MakeChannel(transport);

    std::mutex mutex;
    std::condition_variable cv;
    std::string frame;
    bool got = false;
    channel->SetFrameHandler([&](const uint8_t *data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex);
        frame.assign(reinterpret_cast<const char *>(data), size);
        got = true;
        cv.notify_all();
    });

    channel->Start();
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(3), [&]() { return got; });
    }
    EXPECT_TRUE(got);
    EXPECT_EQ(frame, "abc");
    channel->Stop();
}

TEST(FrameChannelTest, OversizedFrameFailsChannel)
{
    FakeTransport transport;
    transport.in = {9, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto channel = MakeChannel(transport, 4);

    std::atomic<bool> failed{false};
    channel->SetErrorHandler([&](const char *) { failed.store(true); });

    channel->Start();
    EXPECT_TRUE(WaitFor(failed));
    EXPECT_TRUE(channel->IsFailed());
    channel->Stop();
}

TEST(FrameChannelTest, TruncatedFrameFailsChannel)
{
    FakeTransport transport;
    transport.in = {5, 0, 0, 0, 'a'};

    auto channel = MakeChannel(transport);

    std::atomic<bool> failed{false};
    channel->SetErrorHandler([&](const char *) { failed.store(true); });

    channel->Start();
    EXPECT_TRUE(WaitFor(failed));
    channel->Stop();
}

TEST(FrameChannelTest, StopJoinsReaderThatNeverReachesEof)
{
    // Reader keeps returning "no data yet" (never eof); Stop must still join promptly.
    FrameChannel channel(
        [](uint8_t *, size_t, bool &eof) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            eof = false;
            return static_cast<size_t>(0);
        },
        [](const uint8_t *, size_t) { return true; });

    channel.Start();
    const auto start = std::chrono::steady_clock::now();
    channel.Stop();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    EXPECT_LT(elapsed.count(), 2000);
    EXPECT_FALSE(channel.IsRunning());
}

TEST(FrameChannelTest, LogsAreSeparateFromFrames)
{
    FakeTransport transport;
    transport.in = {3, 0, 0, 0, 'a', 'b', 'c'};
    transport.logIn = {'l', 'o', 'g', '\n'};

    auto channel = MakeChannel(transport);

    std::mutex mutex;
    std::condition_variable cv;
    std::string frame;
    std::string log;
    channel->SetFrameHandler([&](const uint8_t *data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex);
        frame.assign(reinterpret_cast<const char *>(data), size);
        cv.notify_all();
    });
    channel->SetLogHandler([&](const std::string &line) {
        std::lock_guard<std::mutex> lock(mutex);
        log = line;
        cv.notify_all();
    });

    channel->Start();
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(3), [&]() { return !frame.empty() && !log.empty(); });
    }
    EXPECT_EQ(frame, "abc");
    EXPECT_EQ(log, "log");
    channel->Stop();
}
