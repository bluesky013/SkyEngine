//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioClip.h>
#include <audio/AudioTypes.h>
#include <core/template/ReferenceObject.h>
#include <core/util/Uuid.h>
#include <framework/world/Component.h>

#include <string>

namespace sky {

    class SerializationContext;
    class AudioSource;

    struct AudioSourceData {
        Uuid        clip;
        float       volume       = 1.f;
        float       pitch        = 1.f;
        bool        loop         = false;
        bool        playOnStart  = true;
        float       spatialBlend = 1.f;
        float       minDistance  = 1.f;
        float       maxDistance  = 50.f;
        std::string bus          = audio::SFX_BUS;
    };

    class AudioSourceComponent : public ComponentAdaptor<AudioSourceData> {
    public:
        AudioSourceComponent();
        ~AudioSourceComponent() override;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(AudioSourceComponent)

        void SetClip(const Uuid &id);
        const Uuid &GetClip() const { return data.clip; }

        void SetVolume(float value);
        float GetVolume() const { return data.volume; }

        void SetPitch(float value);
        float GetPitch() const { return data.pitch; }

        void SetLoop(bool value);
        bool GetLoop() const { return data.loop; }

        void SetPlayOnStart(bool value);
        bool GetPlayOnStart() const { return data.playOnStart; }

        void SetSpatialBlend(float value);
        float GetSpatialBlend() const { return data.spatialBlend; }

        void SetMinDistance(float value);
        float GetMinDistance() const { return data.minDistance; }

        void SetMaxDistance(float value);
        float GetMaxDistance() const { return data.maxDistance; }

        void SetBus(const std::string &value);
        const std::string &GetBus() const { return data.bus; }

        void SyncTransform();

        AudioSource *GetSource() const { return source; }
        AudioClip *GetClipResource() const { return clip.Get(); }

    private:
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
        void Tick(float time) override;

        void ApplySourceParams();
        void LoadClipResource();

        AudioSource        *source = nullptr;
        CounterPtr<AudioClip> clip;
    };

} // namespace sky
