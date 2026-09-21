//
// Created on 2026/09/21.
//

#pragma once

#include <string>
#include <vector>

namespace sky {

    class AudioBus {
    public:
        explicit AudioBus(std::string busName, AudioBus *parentBus = nullptr);
        virtual ~AudioBus() = default;

        const std::string &GetName() const { return name; }
        AudioBus *GetParent() const { return parent; }
        const std::vector<AudioBus *> &GetChildren() const { return children; }

        void SetParent(AudioBus *parentBus);
        void SetVolume(float value);

        float GetVolume() const { return volume; }
        float GetEffectiveVolume() const { return effectiveVolume; }

    protected:
        virtual void OnVolumeChanged(float effective) {}

    private:
        void RefreshEffectiveVolume();

        std::string            name;
        AudioBus              *parent = nullptr;
        std::vector<AudioBus *> children;

        float volume          = 1.f;
        float effectiveVolume = 1.f;
    };

} // namespace sky
