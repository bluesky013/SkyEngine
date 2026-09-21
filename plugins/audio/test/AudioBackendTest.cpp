//
// Created on 2026/09/21.
//

#include "MinAudioBackend.h"

#include <audio/AudioBus.h>
#include <audio/AudioClip.h>
#include <audio/AudioEngine.h>
#include <audio/AudioListener.h>
#include <audio/AudioRegistry.h>
#include <audio/AudioSource.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

using namespace sky;

namespace {

    // Forces miniaudio's null device so the backend runs headlessly without hardware.
    class HeadlessAudioFactory : public AudioRegistry::Impl {
    public:
        AudioEngine *CreateAudioEngine() override { return new MinAudioEngine(true); }
    };

    std::vector<uint8_t> MakeSineWav(float seconds, uint32_t sampleRate)
    {
        const auto     sampleCount = static_cast<uint32_t>(seconds * static_cast<float>(sampleRate));
        const uint16_t channels    = 1;
        const uint16_t bits        = 16;
        const uint32_t dataSize    = sampleCount * channels * bits / 8;

        std::vector<uint8_t> wav(static_cast<size_t>(44 + dataSize), uint8_t(0));
        auto write = [&wav](size_t offset, const void *src, size_t size) {
            std::memcpy(wav.data() + offset, src, size);
        };

        const uint32_t riffSize   = 36 + dataSize;
        const uint32_t fmtSize    = 16;
        const uint16_t audioFmt   = 1;
        const uint32_t byteRate   = sampleRate * channels * bits / 8;
        const uint16_t blockAlign = static_cast<uint16_t>(channels * bits / 8);

        write(0, "RIFF", 4);
        write(4, &riffSize, 4);
        write(8, "WAVEfmt ", 8);
        write(16, &fmtSize, 4);
        write(20, &audioFmt, 2);
        write(22, &channels, 2);
        write(24, &sampleRate, 4);
        write(28, &byteRate, 4);
        write(32, &blockAlign, 2);
        write(34, &bits, 2);
        write(36, "data", 4);
        write(40, &dataSize, 4);

        for (uint32_t i = 0; i < sampleCount; ++i) {
            const float   t      = static_cast<float>(i) / static_cast<float>(sampleRate);
            const int16_t sample = static_cast<int16_t>(0.25f * 32767.f * std::sin(2.f * 3.14159265f * 440.f * t));
            write(44 + i * 2, &sample, 2);
        }

        return wav;
    }

} // namespace

class AudioBackendTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        auto *registry = AudioRegistry::Get();
        registry->UnRegister();
        registry->Register(new HeadlessAudioFactory());
        engineReady = registry->CreateEngine();
    }

    static void TearDownTestSuite()
    {
        AudioRegistry::Get()->UnRegister();
    }

    static bool engineReady;
};

bool AudioBackendTest::engineReady = false;

TEST_F(AudioBackendTest, EngineAndDefaultBuses)
{
    if (!engineReady) {
        GTEST_SKIP() << "audio engine unavailable";
    }

    auto *engine = AudioRegistry::Get()->GetEngine();
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(engine->IsValid());

    ASSERT_NE(engine->GetMasterBus(), nullptr);
    EXPECT_EQ(engine->GetMasterBus()->GetName(), "master");
    EXPECT_NE(engine->GetBus("music"), nullptr);
    EXPECT_NE(engine->GetBus("sfx"), nullptr);
    EXPECT_NE(engine->GetListener(), nullptr);
}

TEST_F(AudioBackendTest, BusVolumeThroughBackend)
{
    if (!engineReady) {
        GTEST_SKIP() << "audio engine unavailable";
    }

    auto *engine = AudioRegistry::Get()->GetEngine();
    auto *sfx    = engine->GetBus("sfx");
    ASSERT_NE(sfx, nullptr);

    engine->GetMasterBus()->SetVolume(0.5f);
    sfx->SetVolume(0.5f);
    EXPECT_FLOAT_EQ(sfx->GetEffectiveVolume(), 0.25f);

    engine->GetMasterBus()->SetVolume(1.f);
    sfx->SetVolume(1.f);
}

TEST_F(AudioBackendTest, ClipFromEmbeddedData)
{
    if (!engineReady) {
        GTEST_SKIP() << "audio engine unavailable";
    }

    AudioClipDesc desc;
    desc.source   = "embedded.wav";
    desc.loadMode = AudioLoadMode::InMemory;

    CounterPtr<AudioClip> clip(AudioRegistry::Get()->CreateClip(desc));
    ASSERT_NE(clip.Get(), nullptr);

    clip->SetEncodedData(MakeSineWav(0.1f, 44100));
    ASSERT_TRUE(clip->Load());
    EXPECT_TRUE(clip->IsLoaded());
    EXPECT_EQ(clip->GetChannels(), 1u);
    EXPECT_EQ(clip->GetSampleRate(), 44100u);
    EXPECT_NEAR(clip->GetDuration(), 0.1f, 0.01f);
}

TEST_F(AudioBackendTest, PlaybackState)
{
    if (!engineReady) {
        GTEST_SKIP() << "audio engine unavailable";
    }

    auto *engine = AudioRegistry::Get()->GetEngine();

    CounterPtr<AudioClip> clip(engine->CreateClip(AudioClipDesc{}));
    ASSERT_NE(clip.Get(), nullptr);
    clip->SetEncodedData(MakeSineWav(0.2f, 44100));
    ASSERT_TRUE(clip->Load());

    std::unique_ptr<AudioSource> source(engine->CreateSource());
    ASSERT_NE(source.get(), nullptr);

    source->SetClip(clip.Get());
    source->SetVolume(0.5f);
    source->SetPitch(1.2f);
    source->SetLoop(true);
    source->Play();
    EXPECT_TRUE(source->IsPlaying());

    source->Pause();
    EXPECT_FALSE(source->IsPlaying());

    source->Stop();
    EXPECT_FALSE(source->IsPlaying());
}

TEST_F(AudioBackendTest, SpatialSourceAndListener)
{
    if (!engineReady) {
        GTEST_SKIP() << "audio engine unavailable";
    }

    auto *engine = AudioRegistry::Get()->GetEngine();

    AudioClipDesc desc;
    desc.loadMode = AudioLoadMode::Streaming;

    CounterPtr<AudioClip> clip(engine->CreateClip(desc));
    ASSERT_NE(clip.Get(), nullptr);
    clip->SetEncodedData(MakeSineWav(0.2f, 44100));
    ASSERT_TRUE(clip->Load());

    auto *listener = engine->GetListener();
    listener->SetPosition(Vector3(0.f, 0.f, 0.f));
    listener->SetOrientation(Vector3(0.f, 0.f, 1.f), Vector3(0.f, 1.f, 0.f));

    std::unique_ptr<AudioSource> source(engine->CreateSource());
    ASSERT_NE(source.get(), nullptr);

    source->SetClip(clip.Get());
    source->SetBus(engine->GetBus("sfx"));
    source->SetSpatialBlend(1.f);
    source->SetAttenuation(AttenuationModel::Inverse, 1.f, 50.f);
    source->SetPosition(Vector3(0.f, 0.f, 5.f));
    source->SetVelocity(Vector3(0.f, 0.f, -1.f));
    source->SetDopplerFactor(1.f);
    source->Play();
    EXPECT_TRUE(source->IsPlaying());
}
