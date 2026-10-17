#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace sky::aurora {

    // One host producer and one coordinator consumer; Close drains accepted packets before WaitPop returns null.
    // The owner must join the consumer before destroying this queue.
    class FramePacketQueue {
    public:
        FramePacketQueue() = default;
        bool                         Init(uint32_t capacity);
        PacketSubmitResult           TrySubmit(std::unique_ptr<FramePacket> &packet);
        std::unique_ptr<FramePacket> TryPop();
        std::unique_ptr<FramePacket> WaitPop();
        std::unique_ptr<FramePacket> WaitPopFor(std::chrono::milliseconds);
        bool                         IsClosed();
        void                         Close();

    private:
        std::unique_ptr<FramePacket> PopLocked();

        std::mutex                                mutex;
        std::condition_variable                   available;
        std::vector<std::unique_ptr<FramePacket>> packets;
        size_t                                    head         = 0;
        size_t                                    count        = 0;
        uint64_t                                  lastSequence = 0;
        bool                                      closed       = false;
    };

} // namespace sky::aurora
