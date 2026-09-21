//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioBus.h>
#include <audio/AudioClip.h>
#include <audio/AudioEngine.h>
#include <audio/AudioListener.h>
#include <audio/AudioSource.h>

#include <miniaudio.h>

#include <string>

namespace sky {

    class MinAudioBus : public AudioBus {
    public:
        MinAudioBus(ma_engine *engine, const std::string &busName);
        ~MinAudioBus() override;

        ma_sound_group *GetGroup() { return &group; }

    protected:
        void OnVolumeChanged(float effective) override;

    private:
        ma_sound_group group;
        bool           initialized = false;
    };

    class MinAudioListener : public AudioListener {
    public:
        explicit MinAudioListener(ma_engine *engine) : engine(engine) {}

        void SetPosition(const Vector3 &position) override;
        void SetOrientation(const Vector3 &forward, const Vector3 &up) override;

    private:
        ma_engine *engine = nullptr;
    };

    class MinAudioClip : public AudioClip {
    public:
        MinAudioClip(ma_engine *engine, const AudioClipDesc &desc);
        ~MinAudioClip() override;

        bool Load() override;
        void Unload() override;
        bool IsLoaded() const override { return loaded; }

        const std::string &GetSource() const { return desc.source; }
        ma_uint32 GetSoundFlags() const;

    private:
        ma_engine *engine = nullptr;
        bool       loaded = false;
    };

    class MinAudioSource : public AudioSource {
    public:
        explicit MinAudioSource(ma_engine *engine);
        ~MinAudioSource() override;

        void Play() override;
        void Pause() override;
        void Stop() override;
        bool IsPlaying() const override;

        void SetClip(AudioClip *inClip) override;
        void SetVolume(float value) override;
        void SetPitch(float value) override;
        void SetLoop(bool value) override;
        void SetBus(AudioBus *inBus) override;

        void SetSpatialBlend(float blend) override;
        void SetPosition(const Vector3 &position) override;
        void SetVelocity(const Vector3 &velocity) override;
        void SetAttenuation(AttenuationModel model, float minDist, float maxDist) override;
        void SetDopplerFactor(float factor) override;

    private:
        bool EnsureSound();
        void ReleaseSound();
        void ApplyState();

        ma_engine    *engine = nullptr;
        ma_sound      sound;
        ma_decoder    decoder;
        bool          soundValid   = false;
        bool          decoderValid = false;
        MinAudioClip *clip         = nullptr;
        MinAudioBus  *bus          = nullptr;

        float            volume        = 1.f;
        float            pitch         = 1.f;
        bool             loop          = false;
        float            spatialBlend  = 0.f;
        float            minDistance   = 1.f;
        float            maxDistance   = 50.f;
        AttenuationModel attenuation   = AttenuationModel::Inverse;
        float            dopplerFactor = 0.f;
    };

    class MinAudioEngine : public AudioEngine {
    public:
        // nullBackend forces miniaudio's null device, which lets headless tests exercise the
        // full decode/mix path without a physical audio device.
        explicit MinAudioEngine(bool useNullBackend = false);
        ~MinAudioEngine() override;

    protected:
        bool OnInitDevice() override;
        void OnShutdownDevice() override;

        AudioBus *CreateBusImpl(const std::string &busName) override;
        AudioListener *CreateListenerImpl() override;
        AudioSource *CreateSourceImpl() override;
        AudioClip *CreateClipImpl(const AudioClipDesc &desc) override;

    private:
        ma_engine  engine;
        ma_context context;
        bool       nullBackend = false;
        bool       engineValid = false;
        bool       contextValid = false;
    };

} // namespace sky
