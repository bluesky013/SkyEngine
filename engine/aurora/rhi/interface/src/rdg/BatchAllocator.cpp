//
// BatchAllocator implementation.
//

#include <aurora/rdg/BatchAllocator.h>
#include <aurora/rhi/Device.h>

#include <cstring>

namespace sky::aurora {

    bool BatchAllocator::Init(Device *device, uint32_t capacity)
    {
        mDevice   = device;
        mCapacity = capacity;

        mAlignment = device->GetCapability().minUniformBufferOffsetAlignment;
        if (mAlignment == 0) {
            mAlignment = 256;
        }

        Buffer::Descriptor desc{};
        desc.size   = capacity;
        desc.usage  = BufferUsageFlagBit::UNIFORM;
        desc.memory = MemoryType::CPU_TO_GPU;
        mBuffer = device->CreateBuffer(desc);
        if (mBuffer == nullptr) {
            return false;
        }

        mMapped = mBuffer->Map();
        return mMapped != nullptr;
    }

    uint32_t BatchAllocator::Allocate(uint32_t size)
    {
        const uint32_t alignedCursor = (mCursor + mAlignment - 1u) / mAlignment * mAlignment;
        const uint32_t end = alignedCursor + size;
        if (end > mCapacity) {
            return UINT32_MAX;
        }
        mCursor = end;
        return alignedCursor;
    }

    void BatchAllocator::Write(uint32_t offset, const void *data, uint32_t size)
    {
        if (mMapped == nullptr || offset + size > mCapacity) {
            return;
        }
        std::memcpy(mMapped + offset, data, size);
    }

    void BatchAllocator::Reset()
    {
        mCursor = 0;
    }

} // namespace sky::aurora
