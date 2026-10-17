#include "AuroraTestHelper.h"

#include <aurora/rhi/Queue.h>

#include <vector>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
namespace {
    class TestStream : public IUploadStream {
    public:
        TestStream(const void *data, uint64_t size) : data(static_cast<const uint8_t *>(data)), size(size) {}
        const uint8_t *Data(uint64_t offset) override
        {
            return data + offset;
        }
        void ReadData(uint64_t offset, uint64_t inSize, uint8_t *out) override
        {
            for (uint64_t i = 0; i < inSize; ++i)
                out[i] = data[offset + i];
        }

    private:
        const uint8_t *data;
        uint64_t       size;
    };
} // namespace

TEST_F(AuroraMetalTest, UploadBufferRoundTrip)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc;
    desc.size   = 256;
    desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    desc.memory = MemoryType::CPU_TO_GPU;
    BufferPtr buffer(device->CreateBuffer(desc));
    ASSERT_NE(buffer, nullptr);

    uint32_t src[64];
    for (uint32_t i = 0; i < 64; ++i)
        src[i] = i * 3;

    auto *queue = device->GetQueue(QueueType::TRANSFER);
    ASSERT_NE(queue, nullptr);
    BufferUploadRequest request;
    request.source    = CounterPtr<IUploadStream>(new TestStream(src, sizeof(src)));
    request.size      = sizeof(src);
    request.dstOffset = 0;
    const auto handle = queue->UploadBuffer(buffer.Get(), {request});
    queue->Wait(handle);
    EXPECT_TRUE(queue->HasComplete(handle));

    auto *mapped = buffer->Map();
    ASSERT_NE(mapped, nullptr);
    const auto *readback = reinterpret_cast<const uint32_t *>(mapped);
    for (uint32_t i = 0; i < 64; ++i)
        EXPECT_EQ(readback[i], i * 3);
}

TEST_F(AuroraMetalTest, UploadsManyBuffersWithoutLosingStaging)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    auto *queue = device->GetQueue(QueueType::TRANSFER);
    ASSERT_NE(queue, nullptr);

    constexpr uint32_t count = 32;
    std::vector<BufferPtr> buffers;
    std::vector<TransferTaskHandle> handles;
    uint32_t data[16];
    for (uint32_t i = 0; i < 16; ++i)
        data[i] = i;
    for (uint32_t i = 0; i < count; ++i) {
        Buffer::Descriptor desc;
        desc.size   = sizeof(data);
        desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
        desc.memory = MemoryType::CPU_TO_GPU;
        BufferPtr buffer(device->CreateBuffer(desc));
        ASSERT_NE(buffer, nullptr);
        BufferUploadRequest request;
        request.source    = CounterPtr<IUploadStream>(new TestStream(data, sizeof(data)));
        request.size      = sizeof(data);
        request.dstOffset = 0;
        handles.push_back(queue->UploadBuffer(buffer.Get(), {request}));
        buffers.push_back(std::move(buffer));
    }
    for (auto handle : handles)
        queue->Wait(handle);
    for (auto &buffer : buffers) {
        auto *mapped = buffer->Map();
        ASSERT_NE(mapped, nullptr);
        const auto *readback = reinterpret_cast<const uint32_t *>(mapped);
        for (uint32_t i = 0; i < 16; ++i)
            EXPECT_EQ(readback[i], i);
    }
}

TEST_F(AuroraMetalTest, UploadImageAndWait)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc;
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.extent      = {8, 8, 1};
    desc.mipLevels   = 1;
    desc.arrayLayers = 1;
    desc.samples     = SampleCount::X1;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory      = MemoryType::GPU_ONLY;
    ImagePtr image(device->CreateImage(desc));
    ASSERT_NE(image, nullptr);

    std::vector<uint8_t> pixels(8 * 8 * 4);
    for (size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<uint8_t>(i);

    auto *queue = device->GetQueue(QueueType::TRANSFER);
    ASSERT_NE(queue, nullptr);
    ImageUploadRequest request;
    request.source      = CounterPtr<IUploadStream>(new TestStream(pixels.data(), pixels.size()));
    request.size        = pixels.size();
    request.mipLevel    = 0;
    request.layer       = 0;
    request.imageExtent = {8, 8, 1};
    const auto handle   = queue->UploadImage(image.Get(), {request});
    queue->Wait(handle);
    EXPECT_TRUE(queue->HasComplete(handle));
}
#endif
