//
// Created on 2026/09/21.
//

#include <audio/AudioBus.h>
#include <audio/AudioClip.h>
#include <audio/AudioImport.h>
#include <audio/AudioRegistry.h>
#include <gtest/gtest.h>

using namespace sky;

namespace {

    class TestClip : public AudioClip {
    public:
        bool Load() override
        {
            loaded = true;
            return true;
        }
        void Unload() override { loaded = false; }
        bool IsLoaded() const override { return loaded; }

    private:
        bool loaded = false;
    };

} // namespace

TEST(AudioTest, NullBackendContract)
{
    auto *registry = AudioRegistry::Get();
    registry->UnRegister();

    EXPECT_FALSE(registry->HasBackend());
    EXPECT_FALSE(registry->CreateEngine());
    EXPECT_EQ(registry->GetEngine(), nullptr);
    EXPECT_EQ(registry->CreateSource(), nullptr);
    EXPECT_EQ(registry->CreateClip(AudioClipDesc{}), nullptr);
}

TEST(AudioTest, BusVolumePropagates)
{
    AudioBus master("master");
    AudioBus child("sfx", &master);
    AudioBus grandChild("reverb", &child);

    master.SetVolume(0.5f);
    child.SetVolume(0.5f);
    grandChild.SetVolume(0.5f);

    EXPECT_FLOAT_EQ(master.GetEffectiveVolume(), 0.5f);
    EXPECT_FLOAT_EQ(child.GetEffectiveVolume(), 0.25f);
    EXPECT_FLOAT_EQ(grandChild.GetEffectiveVolume(), 0.125f);

    child.SetVolume(0.f);
    EXPECT_FLOAT_EQ(child.GetEffectiveVolume(), 0.f);
    EXPECT_FLOAT_EQ(grandChild.GetEffectiveVolume(), 0.f);
}

TEST(AudioTest, BusReparentUpdatesEffectiveVolume)
{
    AudioBus quiet("quiet");
    quiet.SetVolume(0.25f);

    AudioBus loud("loud");
    loud.SetVolume(0.8f);

    AudioBus bus("bus", &quiet);
    EXPECT_FLOAT_EQ(bus.GetEffectiveVolume(), 0.25f);

    bus.SetParent(&loud);
    EXPECT_FLOAT_EQ(bus.GetEffectiveVolume(), 0.8f);
}

TEST(AudioTest, ClipMetadata)
{
    TestClip     clip;
    AudioClipDesc desc;
    desc.duration   = 3.5f;
    desc.channels   = 2;
    desc.sampleRate = 48000;
    desc.loadMode   = AudioLoadMode::Streaming;
    desc.defaultBus = "music";
    desc.loop       = true;
    clip.SetDesc(desc);

    EXPECT_FLOAT_EQ(clip.GetDuration(), 3.5f);
    EXPECT_EQ(clip.GetChannels(), 2u);
    EXPECT_EQ(clip.GetSampleRate(), 48000u);
    EXPECT_EQ(clip.GetLoadMode(), AudioLoadMode::Streaming);
    EXPECT_EQ(clip.GetDefaultBus(), "music");
    EXPECT_TRUE(clip.IsLoop());
    EXPECT_TRUE(clip.Load());
    EXPECT_TRUE(clip.IsLoaded());
}

TEST(AudioTest, SourceFormatValidation)
{
    EXPECT_TRUE(audio::IsSupportedSourceFormat("shot.wav"));
    EXPECT_TRUE(audio::IsSupportedSourceFormat("music.OGG"));
    EXPECT_TRUE(audio::IsSupportedSourceFormat("a/b/theme.Flac"));
    EXPECT_FALSE(audio::IsSupportedSourceFormat("clip.aiff"));
    EXPECT_FALSE(audio::IsSupportedSourceFormat("noext"));
}
