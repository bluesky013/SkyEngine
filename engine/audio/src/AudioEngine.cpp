//
// Created on 2026/09/21.
//

#include <audio/AudioEngine.h>
#include <audio/AudioBus.h>
#include <audio/AudioClip.h>
#include <audio/AudioListener.h>
#include <audio/AudioSource.h>

namespace sky {

    AudioEngine::AudioEngine() = default;

    AudioEngine::~AudioEngine() = default;

    bool AudioEngine::Init()
    {
        if (valid) {
            return true;
        }

        if (!OnInitDevice()) {
            return false;
        }

        // The master bus is the root of the bus tree; named buses hang off it.
        masterBus.reset(CreateBusImpl(audio::MASTER_BUS));
        listener.reset(CreateListenerImpl());

        CreateBus(audio::MUSIC_BUS, masterBus.get());
        CreateBus(audio::SFX_BUS, masterBus.get());

        valid = true;
        return true;
    }

    void AudioEngine::Shutdown()
    {
        if (!valid) {
            return;
        }

        buses.clear();
        listener.reset();
        masterBus.reset();

        OnShutdownDevice();
        valid = false;
    }

    AudioBus *AudioEngine::GetBus(const std::string &busName) const
    {
        auto iter = buses.find(busName);
        return iter != buses.end() ? iter->second.get() : nullptr;
    }

    AudioBus *AudioEngine::CreateBus(const std::string &busName, AudioBus *parent)
    {
        if (auto *existing = GetBus(busName); existing != nullptr) {
            return existing;
        }

        AudioBus *bus = CreateBusImpl(busName);
        if (bus == nullptr) {
            return nullptr;
        }

        bus->SetParent(parent != nullptr ? parent : masterBus.get());
        buses.emplace(busName, std::unique_ptr<AudioBus>(bus));
        return bus;
    }

    AudioSource *AudioEngine::CreateSource()
    {
        return valid ? CreateSourceImpl() : nullptr;
    }

    AudioClip *AudioEngine::CreateClip(const AudioClipDesc &desc)
    {
        return CreateClipImpl(desc);
    }

} // namespace sky
