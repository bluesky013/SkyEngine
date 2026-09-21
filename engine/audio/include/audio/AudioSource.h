//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioTypes.h>
#include <core/math/Vector3.h>

namespace sky {

    class AudioBus;
    class AudioClip;

    class AudioSource {
    public:
        AudioSource()          = default;
        virtual ~AudioSource() = default;

        virtual void Play()              = 0;
        virtual void Pause()             = 0;
        virtual void Stop()              = 0;
        virtual bool IsPlaying() const   = 0;

        virtual void SetClip(AudioClip *clip) = 0;
        virtual void SetVolume(float volume)  = 0;
        virtual void SetPitch(float pitch)    = 0;
        virtual void SetLoop(bool loop)       = 0;
        virtual void SetBus(AudioBus *bus)    = 0;

        virtual void SetSpatialBlend(float blend) = 0;
        virtual void SetPosition(const Vector3 &position) = 0;
        virtual void SetVelocity(const Vector3 &velocity) = 0;
        virtual void SetAttenuation(AttenuationModel model, float minDistance, float maxDistance) = 0;
        virtual void SetDopplerFactor(float factor) = 0;
    };

} // namespace sky
