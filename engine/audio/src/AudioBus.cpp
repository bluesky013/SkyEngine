//
// Created on 2026/09/21.
//

#include <audio/AudioBus.h>

#include <algorithm>

namespace sky {

    AudioBus::AudioBus(std::string busName, AudioBus *parentBus) : name(std::move(busName))
    {
        if (parentBus != nullptr) {
            SetParent(parentBus);
        }
    }

    void AudioBus::SetParent(AudioBus *parentBus)
    {
        if (parent == parentBus) {
            return;
        }

        if (parent != nullptr) {
            auto &list = parent->children;
            list.erase(std::remove(list.begin(), list.end(), this), list.end());
        }

        parent = parentBus;

        if (parent != nullptr) {
            // The parent owns the child list; a bus is never re-parented across engines.
            parent->children.emplace_back(this);
        }

        RefreshEffectiveVolume();
    }

    void AudioBus::SetVolume(float value)
    {
        if (volume == value) {
            return;
        }
        volume = value;
        RefreshEffectiveVolume();
    }

    void AudioBus::RefreshEffectiveVolume()
    {
        const float base = parent != nullptr ? parent->effectiveVolume : 1.f;
        effectiveVolume  = base * volume;

        OnVolumeChanged(effectiveVolume);

        for (auto *child : children) {
            child->RefreshEffectiveVolume();
        }
    }

} // namespace sky
