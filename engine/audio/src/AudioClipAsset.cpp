//
// Created on 2026/09/21.
//

#include <audio/AudioClipAsset.h>
#include <audio/AudioRegistry.h>
#include <framework/serialization/BinaryArchive.h>

#include <core/logger/Logger.h>

static const char *TAG = "AudioClipAsset";

namespace sky {

    void AudioClipData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(desc.source);
        archive.LoadValue(desc.loadMode);
        archive.LoadValue(desc.defaultBus);
        archive.LoadValue(desc.loop);
        archive.LoadValue(desc.duration);
        archive.LoadValue(desc.channels);
        archive.LoadValue(desc.sampleRate);

        uint32_t size = 0;
        archive.LoadValue(size);
        rawData.storage.resize(size);
        if (size > 0) {
            archive.LoadValue(reinterpret_cast<char *>(rawData.storage.data()), size);
        }
    }

    void AudioClipData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(desc.source);
        archive.SaveValue(desc.loadMode);
        archive.SaveValue(desc.defaultBus);
        archive.SaveValue(desc.loop);
        archive.SaveValue(desc.duration);
        archive.SaveValue(desc.channels);
        archive.SaveValue(desc.sampleRate);

        const auto size = static_cast<uint32_t>(rawData.storage.size());
        archive.SaveValue(size);
        if (size > 0) {
            archive.SaveValue(reinterpret_cast<const char *>(rawData.storage.data()), size);
        }
    }

    CounterPtr<AudioClip> CreateAudioClipFromAsset(const AudioClipAssetPtr &asset)
    {
        if (asset == nullptr) {
            return {};
        }

        return asset->GetOrCreateResource([](Asset<AudioClip> &inAsset) -> CounterPtr<AudioClip> {
            const auto &data = inAsset.Data();

            auto *clip = AudioRegistry::Get()->CreateClip(data.desc);
            if (clip == nullptr) {
                LOG_W(TAG, "No audio backend, clip asset stays silent.");
                return {};
            }

            clip->SetEncodedData(data.rawData.storage);
            if (!clip->Load()) {
                LOG_W(TAG, "Failed to load audio clip: %s", data.desc.source.c_str());
                delete clip;
                return {};
            }

            return CounterPtr<AudioClip>(clip);
        });
    }

} // namespace sky
