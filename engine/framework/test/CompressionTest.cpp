#include <framework/compression/Compressor.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <future>
#include <limits>
#include <vector>

using namespace sky;

namespace {
    struct ProviderLifetime {
        std::atomic_uint32_t destroyed{0};
        std::atomic_bool     leaseAlive{true};
        std::atomic_bool     providerDestroyedAfterLease{false};
    };

    struct ProviderLease {
        explicit ProviderLease(std::shared_ptr<ProviderLifetime> state) : state(std::move(state))
        {
        }
        ~ProviderLease()
        {
            state->leaseAlive.store(false);
        }
        std::shared_ptr<ProviderLifetime> state;
    };

    class TrackingCompressor final : public ICompressor {
    public:
        explicit TrackingCompressor(std::shared_ptr<ProviderLifetime> state) : state(std::move(state))
        {
        }
        ~TrackingCompressor() override
        {
            state->providerDestroyedAfterLease.store(!state->leaseAlive.load());
            state->destroyed.fetch_add(1);
        }
        uint32_t CompressBound(uint32_t size) override
        {
            return size;
        }
        CompressResult Compress(const std::span<const uint8_t> &input, const std::span<uint8_t> &output, uint32_t flags) override
        {
            if (flags || output.size() < input.size() || input.size() > std::numeric_limits<uint32_t>::max())
                return {false, 0};
            std::ranges::copy(input, output.begin());
            return {true, static_cast<uint32_t>(input.size())};
        }
        CompressResult DeCompress(const std::span<const uint8_t> &input, const std::span<uint8_t> &output, uint32_t flags) override
        {
            return Compress(input, output, flags);
        }

    private:
        std::shared_ptr<ProviderLifetime> state;
    };

    class CompressionTest : public testing::Test {
    protected:
        void SetUp() override
        {
            manager = CompressionManager::Get();
            Reset();
        }
        void TearDown() override
        {
            Reset();
        }
        void Reset()
        {
            for (const auto method : {CompressionMethod::LZ4, CompressionMethod::ZLIB}) {
                manager->UnRegister(method);
                EXPECT_TRUE(manager->SetEnabled(method, true));
            }
        }
        CompressionManager *manager = nullptr;
    };
} // namespace

TEST_F(CompressionTest, BuiltinCodecIsAvailableWithoutLoadingAModule)
{
    const bool enabled = SKY_FRAMEWORK_EXPECT_LZ4 != 0;
    EXPECT_EQ(manager->AcquireCompressor(CompressionMethod::LZ4) != nullptr, enabled);
    EXPECT_EQ(manager->AcquireCompressor(CompressionMethod::ZLIB), nullptr);
}

TEST_F(CompressionTest, OverrideDoesNotDestroyTheBuiltinAndDuplicateRegistrationFails)
{
    const auto builtin = manager->AcquireCompressor(CompressionMethod::LZ4);
    const auto state   = std::make_shared<ProviderLifetime>();
    ASSERT_TRUE(manager->Register(CompressionMethod::LZ4, new TrackingCompressor(state)));
    EXPECT_NE(manager->AcquireCompressor(CompressionMethod::LZ4), builtin);
    EXPECT_FALSE(manager->Register(CompressionMethod::LZ4, new TrackingCompressor(state)));
    EXPECT_EQ(state->destroyed.load(), 1u);
    manager->UnRegister(CompressionMethod::LZ4);
    EXPECT_EQ(state->destroyed.load(), 2u);
    EXPECT_EQ(manager->AcquireCompressor(CompressionMethod::LZ4), builtin);
}

TEST_F(CompressionTest, AcquiredProviderRetainsItsObjectAndCodeUntilReleased)
{
    const auto                         state = std::make_shared<ProviderLifetime>();
    auto                               lease = std::make_shared<ProviderLease>(state);
    const std::weak_ptr<ProviderLease> weak  = lease;
    ASSERT_TRUE(manager->Register(CompressionMethod::ZLIB, new TrackingCompressor(state), lease));
    lease.reset();
    auto acquired = manager->AcquireCompressor(CompressionMethod::ZLIB);
    manager->UnRegister(CompressionMethod::ZLIB);
    EXPECT_EQ(manager->AcquireCompressor(CompressionMethod::ZLIB), nullptr);
    EXPECT_EQ(state->destroyed.load(), 0u);
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(acquired->CompressBound(17), 17u);
    acquired.reset();
    EXPECT_EQ(state->destroyed.load(), 1u);
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(state->providerDestroyedAfterLease.load());
}

TEST_F(CompressionTest, DisabledSelectionDoesNotInvalidateAnAcquiredProvider)
{
    const auto state = std::make_shared<ProviderLifetime>();
    ASSERT_TRUE(manager->Register(CompressionMethod::ZLIB, new TrackingCompressor(state)));
    const auto acquired = manager->AcquireCompressor(CompressionMethod::ZLIB);
    ASSERT_TRUE(manager->SetEnabled(CompressionMethod::ZLIB, false));
    EXPECT_EQ(manager->AcquireCompressor(CompressionMethod::ZLIB), nullptr);
    EXPECT_EQ(acquired->CompressBound(9), 9u);
    ASSERT_TRUE(manager->SetEnabled(CompressionMethod::ZLIB, true));
    EXPECT_EQ(manager->AcquireCompressor(CompressionMethod::ZLIB), acquired);
    EXPECT_FALSE(manager->SetEnabled(static_cast<CompressionMethod>(99), true));
    EXPECT_FALSE(manager->Register(CompressionMethod::LZ4, nullptr));
    EXPECT_EQ(manager->AcquireCompressor(static_cast<CompressionMethod>(99)), nullptr);
}

TEST_F(CompressionTest, BuiltinRoundTripRejectsTooSmallBuffersAndUnsupportedFlags)
{
    if (!SKY_FRAMEWORK_EXPECT_LZ4)
        GTEST_SKIP() << "The Framework builtin LZ4 codec is disabled";
    const auto codec = manager->AcquireCompressor(CompressionMethod::LZ4);
    ASSERT_NE(codec, nullptr);
    std::vector<uint8_t> input(8192);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<uint8_t>((i * 131 + i / 7) & 255);
    const auto bound = codec->CompressBound(static_cast<uint32_t>(input.size()));
    ASSERT_GT(bound, 0u);
    std::vector<uint8_t> encoded(bound), decoded(input.size());
    const auto           compressed = codec->Compress(input, encoded, 0);
    ASSERT_TRUE(compressed.first);
    ASSERT_GT(compressed.second, 0u);
    encoded.resize(compressed.second);
    const auto restored = codec->DeCompress(encoded, decoded, 0);
    ASSERT_TRUE(restored.first);
    EXPECT_EQ(restored.second, input.size());
    EXPECT_EQ(decoded, input);
    EXPECT_FALSE(codec->Compress(input, {}, 0).first);
    std::array<uint8_t, 1> tiny{};
    EXPECT_FALSE(codec->Compress(input, tiny, 0).first);
    EXPECT_FALSE(codec->DeCompress(encoded, tiny, 0).first);
    EXPECT_FALSE(codec->DeCompress({}, decoded, 0).first);
    EXPECT_FALSE(codec->Compress(input, decoded, 1).first);
    EXPECT_FALSE(codec->DeCompress(encoded, decoded, 1).first);
    EXPECT_EQ(codec->CompressBound(std::numeric_limits<uint32_t>::max()), 0u);
}

TEST_F(CompressionTest, BuiltinSupportsEmptyDataAndIndependentParallelCalls)
{
    if (!SKY_FRAMEWORK_EXPECT_LZ4)
        GTEST_SKIP() << "The Framework builtin LZ4 codec is disabled";
    const auto codec = manager->AcquireCompressor(CompressionMethod::LZ4);
    ASSERT_NE(codec, nullptr);
    std::vector<uint8_t> encoded(codec->CompressBound(0));
    const auto           compressed = codec->Compress({}, encoded, 0);
    ASSERT_TRUE(compressed.first);
    encoded.resize(compressed.second);
    const auto empty = codec->DeCompress(encoded, {}, 0);
    EXPECT_TRUE(empty.first);
    EXPECT_EQ(empty.second, 0u);

    std::array<std::future<bool>, 4> jobs;
    for (uint32_t worker = 0; worker < jobs.size(); ++worker) {
        jobs[worker] = std::async(std::launch::async, [codec, worker] {
            std::vector<uint8_t> input(4096, static_cast<uint8_t>(worker)), restored(input.size());
            std::vector<uint8_t> compressed(codec->CompressBound(static_cast<uint32_t>(input.size())));
            for (uint32_t iteration = 0; iteration < 32; ++iteration) {
                const auto result = codec->Compress(input, compressed, 0);
                if (!result.first)
                    return false;
                const auto decoded = codec->DeCompress(std::span(compressed).first(result.second), restored, 0);
                if (!decoded.first || decoded.second != input.size() || restored != input)
                    return false;
            }
            return true;
        });
    }
    for (auto &job : jobs)
        EXPECT_TRUE(job.get());
}
