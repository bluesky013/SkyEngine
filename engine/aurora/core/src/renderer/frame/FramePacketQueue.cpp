#include "frame/FramePacketQueue.h"

namespace sky::aurora {

    bool FramePacketQueue::Init(uint32_t capacity)
    {
        std::lock_guard lock(mutex);
        if (!capacity || !packets.empty() || closed)
            return false;
        packets.resize(capacity);
        return true;
    }

    PacketSubmitResult FramePacketQueue::TrySubmit(std::unique_ptr<FramePacket> &packet)
    {
        std::unique_lock lock(mutex);
        if (packets.empty())
            return {PacketSubmitStatus::REJECTED, {RendererErrorCode::NOT_READY, "Packet queue is not initialized"}};
        if (closed) {
            return {PacketSubmitStatus::REJECTED, {RendererErrorCode::NOT_READY, "Packet queue is closed"}};
        }
        if (!packet || packet->sequence == 0 || packet->sequence <= lastSequence) {
            return {PacketSubmitStatus::REJECTED, {RendererErrorCode::INVALID_PACKET, "Packet sequence must increase and be nonzero"}};
        }
        if (count == packets.size()) {
            return {PacketSubmitStatus::BUSY};
        }
        lastSequence                             = packet->sequence;
        packets[(head + count) % packets.size()] = std::move(packet);
        ++count;
        lock.unlock();
        available.notify_one();
        return {PacketSubmitStatus::ACCEPTED};
    }

    std::unique_ptr<FramePacket> FramePacketQueue::PopLocked()
    {
        if (count == 0) {
            return {};
        }
        auto packet = std::move(packets[head]);
        head        = (head + 1) % packets.size();
        --count;
        return packet;
    }

    std::unique_ptr<FramePacket> FramePacketQueue::TryPop()
    {
        std::lock_guard lock(mutex);
        return PopLocked();
    }

    std::unique_ptr<FramePacket> FramePacketQueue::WaitPop()
    {
        std::unique_lock lock(mutex);
        available.wait(lock, [&] { return closed || count != 0 || packets.empty(); });
        return PopLocked();
    }

    void FramePacketQueue::Close()
    {
        {
            std::lock_guard lock(mutex);
            closed = true;
        }
        available.notify_all();
    }

    std::unique_ptr<FramePacket> FramePacketQueue::WaitPopFor(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mutex);
        available.wait_for(lock, timeout, [&] { return closed || count != 0 || packets.empty(); });
        return PopLocked();
    }
    bool FramePacketQueue::IsClosed()
    {
        std::lock_guard lock(mutex);
        return closed;
    }

} // namespace sky::aurora
