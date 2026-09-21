//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioTypes.h>
#include <framework/world/World.h>

#include <memory>
#include <vector>

namespace sky {

    class AudioClip;
    class AudioEngine;
    class AudioSource;

    class AudioSystem : public IWorldSubSystem {
    public:
        AudioSystem();
        ~AudioSystem() override;

        static constexpr std::string_view NAME = "Audio";

        AudioEngine *GetEngine() const { return engine; }

        AudioSource *Play(AudioClip *clip, const AudioPlaybackDesc &desc = {});

    private:
        void OnAttachToWorld(World &inWorld) override;
        void OnDetachFromWorld(World &inWorld) override;
        void Tick(float time) override;

        void UpdateListener(World &inWorld);

        AudioEngine *engine = nullptr;
        World       *world  = nullptr;

        std::vector<std::unique_ptr<AudioSource>> sources;
    };

} // namespace sky
