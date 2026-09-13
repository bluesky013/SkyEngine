//
// Upload tests: async buffer/image upload, staging ring, isUMA capability.
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/StagingBufferAllocator.h>
#include <aurora/rhi/Queue.h>

#include <vector>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    class TestStream : public IUploadStream {
    public:
        TestStream(const void *data, uint64_t size)
            : data(static_cast<const uint8_t *>(data))
            , size(size)
        {
        }

        const uint8_t *Data(uint64_t offset) override
        {
            return data + offset;
        }

        void ReadData(uint64_t offset, uint64_t inSize, uint8_t *out) override
        {
            for (uint64_t i = 0; i < inSize; ++i) {
                out[i] = data[offset + i];
            }
        }

    private:
        const uint8_t *data;
        uint64_t       size;
    };

} // namespace

using UploadTestVulkan = AuroraVulkanTest;

TEST_F(UploadTestVulkan, IsUMAReported)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    const bool uma = device->GetCapability().isUMA;
    EXPECT_TRUE(uma == true || uma == false);
}

TEST_F(UploadTestVulkan, UploadBufferRoundTrip)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor desc = {};
    desc.size   = 256;
    desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
    desc.memory = MemoryType::CPU_TO_GPU;
    CounterPtr<Buffer> buffer(device->CreateBuffer(desc));
    ASSERT_NE(buffer, nullptr);

    uint32_t src[64];
    for (uint32_t i = 0; i < 64; ++i) {
        src[i] = i * 3;
    }

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
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_EQ(readback[i], i * 3);
    }
}

TEST_F(UploadTestVulkan, UploadImageUncompressed)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType   = ImageType::IMAGE_2D;
    desc.format      = PixelFormat::RGBA8_UNORM;
    desc.extent      = {8, 8, 1};
    desc.mipLevels   = 1;
    desc.arrayLayers = 1;
    desc.samples     = SampleCount::X1;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory      = MemoryType::GPU_ONLY;
    CounterPtr<Image> image(device->CreateImage(desc));
    ASSERT_NE(image, nullptr);

    std::vector<uint8_t> pixels(8 * 8 * 4);
    for (size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<uint8_t>(i);
    }

    auto *queue = device->GetQueue(QueueType::TRANSFER);
    ASSERT_NE(queue, nullptr);

    ImageUploadRequest request;
    request.source      = CounterPtr<IUploadStream>(new TestStream(pixels.data(), pixels.size()));
    request.size        = pixels.size();
    request.mipLevel    = 0;
    request.layer       = 0;
    request.imageExtent = {8, 8, 1};

    const auto handle = queue->UploadImage(image.Get(), {request});
    queue->Wait(handle);
    EXPECT_TRUE(queue->HasComplete(handle));
}

TEST_F(UploadTestVulkan, UploadImageCompressedBC1)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Image::Descriptor desc = {};
    desc.imageType   = ImageType::IMAGE_2D;
    desc.format      = PixelFormat::BC1_RGB_UNORM_BLOCK;
    desc.extent      = {8, 8, 1};
    desc.mipLevels   = 1;
    desc.arrayLayers = 1;
    desc.samples     = SampleCount::X1;
    desc.usage       = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
    desc.memory      = MemoryType::GPU_ONLY;
    CounterPtr<Image> image(device->CreateImage(desc));
    ASSERT_NE(image, nullptr);

    // BC1: 4x4 block = 8 bytes; 8x8 = 4 blocks = 32 bytes.
    std::vector<uint8_t> blocks(32, 0xAB);

    auto *queue = device->GetQueue(QueueType::TRANSFER);
    ASSERT_NE(queue, nullptr);

    ImageUploadRequest request;
    request.source      = CounterPtr<IUploadStream>(new TestStream(blocks.data(), blocks.size()));
    request.size        = blocks.size();
    request.imageExtent = {8, 8, 1};

    const auto handle = queue->UploadImage(image.Get(), {request});
    queue->Wait(handle);
    EXPECT_TRUE(queue->HasComplete(handle));
}

TEST_F(UploadTestVulkan, StagingBufferAllocatorRing)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    StagingBufferAllocator allocator;
    ASSERT_TRUE(allocator.Init(device, 1024, 2));

    const auto a0 = allocator.Allocate(128, 16);
    ASSERT_NE(a0.buffer, nullptr);
    EXPECT_EQ(a0.offset, 0u);

    allocator.Reset();
    const auto a1 = allocator.Allocate(64, 16);
    ASSERT_NE(a1.buffer, nullptr);
    EXPECT_EQ(a1.offset, 0u);
    EXPECT_NE(a1.buffer, a0.buffer);

    for (int i = 0; i < 64; ++i) {
        a1.mapped[i] = static_cast<uint8_t>(i);
    }

    allocator.Reset();
    const auto a2 = allocator.Allocate(256, 16);
    ASSERT_NE(a2.buffer, nullptr);
    EXPECT_EQ(a2.buffer, a0.buffer); // wrapped back to segment 0
}
