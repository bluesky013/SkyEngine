//
// Created on 2026/09/21.
//

#include "MinAudioBackend.h"

#include <core/logger/Logger.h>

static const char *TAG = "MinAudio";

namespace sky {

    namespace {
        ma_attenuation_model ToMaAttenuation(AttenuationModel model)
        {
            switch (model) {
                case AttenuationModel::None:        return ma_attenuation_model_none;
                case AttenuationModel::Linear:      return ma_attenuation_model_linear;
                case AttenuationModel::Exponential: return ma_attenuation_model_exponential;
                case AttenuationModel::Inverse:
                default:                            return ma_attenuation_model_inverse;
            }
        }
    } // namespace

    MinAudioBus::MinAudioBus(ma_engine *engine, const std::string &busName) : AudioBus(busName)
    {
        initialized = ma_sound_group_init(engine, MA_SOUND_FLAG_DECODE, nullptr, &group) == MA_SUCCESS;
        if (!initialized) {
            LOG_W(TAG, "Failed to create sound group for bus '%s'.", busName.c_str());
        }
    }

    MinAudioBus::~MinAudioBus()
    {
        if (initialized) {
            ma_sound_group_uninit(&group);
        }
    }

    void MinAudioBus::OnVolumeChanged(float effective)
    {
        if (initialized) {
            ma_sound_group_set_volume(&group, effective);
        }
    }

    void MinAudioListener::SetPosition(const Vector3 &position)
    {
        if (engine != nullptr) {
            ma_engine_listener_set_position(engine, 0, position.x, position.y, position.z);
        }
    }

    void MinAudioListener::SetOrientation(const Vector3 &forward, const Vector3 &up)
    {
        if (engine == nullptr) {
            return;
        }
        ma_engine_listener_set_direction(engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(engine, 0, up.x, up.y, up.z);
    }

    MinAudioClip::MinAudioClip(ma_engine *engine, const AudioClipDesc &desc) : engine(engine)
    {
        this->desc = desc;
    }

    MinAudioClip::~MinAudioClip()
    {
        Unload();
    }

    bool MinAudioClip::Load()
    {
        if (loaded) {
            return true;
        }
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        ma_decoder        decoder;

        ma_result result = MA_ERROR;
        if (HasEncodedData()) {
            const auto &bytes = GetEncodedData();
            result = ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder);
        } else if (!desc.source.empty()) {
            result = ma_decoder_init_file(desc.source.c_str(), &config, &decoder);
        } else {
            LOG_W(TAG, "Audio clip has no source.");
            return false;
        }

        if (result != MA_SUCCESS) {
            LOG_W(TAG, "Cannot decode audio clip: %s", desc.source.c_str());
            return false;
        }

        ma_uint64 length = 0;
        ma_decoder_get_length_in_pcm_frames(&decoder, &length);

        desc.sampleRate = decoder.outputSampleRate;
        desc.channels   = decoder.outputChannels;
        if (decoder.outputSampleRate != 0) {
            desc.duration = static_cast<float>(length) / static_cast<float>(decoder.outputSampleRate);
        }

        ma_decoder_uninit(&decoder);
        loaded = true;
        return true;
    }

    void MinAudioClip::Unload()
    {
        loaded = false;
    }

    ma_uint32 MinAudioClip::GetSoundFlags() const
    {
        return desc.loadMode == AudioLoadMode::Streaming ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
    }

    MinAudioSource::MinAudioSource(ma_engine *engine) : engine(engine) {}

    MinAudioSource::~MinAudioSource()
    {
        ReleaseSound();
    }

    bool MinAudioSource::EnsureSound()
    {
        if (soundValid) {
            return true;
        }
        if (engine == nullptr || clip == nullptr || !clip->IsLoaded()) {
            return false;
        }

        ma_uint32 flags = clip->GetSoundFlags();
        if (loop) {
            flags |= MA_SOUND_FLAG_LOOPING;
        }

        ma_sound_group *group = bus != nullptr ? bus->GetGroup() : nullptr;

        if (clip->HasEncodedData()) {
            const auto &bytes = clip->GetEncodedData();
            ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
            if (ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder) != MA_SUCCESS) {
                LOG_W(TAG, "Failed to decode embedded audio data.");
                return false;
            }
            decoderValid = true;

            if (ma_sound_init_from_data_source(engine, &decoder, flags, group, &sound) != MA_SUCCESS) {
                ma_decoder_uninit(&decoder);
                decoderValid = false;
                LOG_W(TAG, "Failed to init sound from memory: %s", clip->GetSource().c_str());
                return false;
            }
        } else if (ma_sound_init_from_file(engine, clip->GetSource().c_str(), flags, group, nullptr, &sound) != MA_SUCCESS) {
            LOG_W(TAG, "Failed to init sound: %s", clip->GetSource().c_str());
            return false;
        }

        soundValid = true;
        ApplyState();
        return true;
    }

    void MinAudioSource::ReleaseSound()
    {
        if (soundValid) {
            ma_sound_uninit(&sound);
            soundValid = false;
        }
        if (decoderValid) {
            ma_decoder_uninit(&decoder);
            decoderValid = false;
        }
    }

    void MinAudioSource::ApplyState()
    {
        if (!soundValid) {
            return;
        }

        ma_sound_set_volume(&sound, volume);
        ma_sound_set_pitch(&sound, pitch);
        ma_sound_set_looping(&sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_spatialization_enabled(&sound, spatialBlend > 0.f ? MA_TRUE : MA_FALSE);
        ma_sound_set_attenuation_model(&sound, ToMaAttenuation(attenuation));
        ma_sound_set_min_distance(&sound, minDistance);
        ma_sound_set_max_distance(&sound, maxDistance);
        ma_sound_set_doppler_factor(&sound, dopplerFactor);
    }

    void MinAudioSource::Play()
    {
        if (!EnsureSound()) {
            return;
        }
        ma_sound_start(&sound);
    }

    void MinAudioSource::Pause()
    {
        if (soundValid) {
            ma_sound_stop(&sound);
        }
    }

    void MinAudioSource::Stop()
    {
        if (!soundValid) {
            return;
        }
        ma_sound_stop(&sound);
        ma_sound_seek_to_pcm_frame(&sound, 0);
    }

    bool MinAudioSource::IsPlaying() const
    {
        return soundValid && ma_sound_is_playing(&sound);
    }

    void MinAudioSource::SetClip(AudioClip *inClip)
    {
        ReleaseSound();
        clip = static_cast<MinAudioClip *>(inClip);
    }

    void MinAudioSource::SetVolume(float value)
    {
        volume = value;
        ApplyState();
    }

    void MinAudioSource::SetPitch(float value)
    {
        pitch = value;
        ApplyState();
    }

    void MinAudioSource::SetLoop(bool value)
    {
        loop = value;
        ApplyState();
    }

    void MinAudioSource::SetBus(AudioBus *inBus)
    {
        if (bus == inBus) {
            return;
        }

        const bool wasPlaying = IsPlaying();
        ReleaseSound();
        bus = static_cast<MinAudioBus *>(inBus);
        if (wasPlaying) {
            Play();
        }
    }

    void MinAudioSource::SetSpatialBlend(float blend)
    {
        spatialBlend = blend;
        ApplyState();
    }

    void MinAudioSource::SetPosition(const Vector3 &position)
    {
        if (soundValid) {
            ma_sound_set_position(&sound, position.x, position.y, position.z);
        }
    }

    void MinAudioSource::SetVelocity(const Vector3 &velocity)
    {
        if (soundValid) {
            ma_sound_set_velocity(&sound, velocity.x, velocity.y, velocity.z);
        }
    }

    void MinAudioSource::SetAttenuation(AttenuationModel model, float minDist, float maxDist)
    {
        attenuation = model;
        minDistance = minDist;
        maxDistance = maxDist;
        ApplyState();
    }

    void MinAudioSource::SetDopplerFactor(float factor)
    {
        dopplerFactor = factor;
        ApplyState();
    }

    MinAudioEngine::MinAudioEngine(bool useNullBackend) : nullBackend(useNullBackend) {}

    MinAudioEngine::~MinAudioEngine()
    {
        if (engineValid) {
            ma_engine_uninit(&engine);
            engineValid = false;
        }
        if (contextValid) {
            ma_context_uninit(&context);
            contextValid = false;
        }
    }

    bool MinAudioEngine::OnInitDevice()
    {
        ma_engine_config config = ma_engine_config_init();

        if (nullBackend) {
            ma_context_config contextConfig = ma_context_config_init();
            ma_backend        backends[]    = {ma_backend_null};
            if (ma_context_init(backends, 1, &contextConfig, &context) != MA_SUCCESS) {
                LOG_W(TAG, "Failed to init miniaudio null context.");
                return false;
            }
            contextValid  = true;
            config.pContext = &context;
        }

        if (ma_engine_init(&config, &engine) != MA_SUCCESS) {
            LOG_W(TAG, "Failed to init miniaudio engine, running without audio.");
            return false;
        }

        engineValid = true;
        return true;
    }

    void MinAudioEngine::OnShutdownDevice()
    {
        if (engineValid) {
            ma_engine_uninit(&engine);
            engineValid = false;
        }
        if (contextValid) {
            ma_context_uninit(&context);
            contextValid = false;
        }
    }

    AudioBus *MinAudioEngine::CreateBusImpl(const std::string &busName)
    {
        return new MinAudioBus(&engine, busName);
    }

    AudioListener *MinAudioEngine::CreateListenerImpl()
    {
        return new MinAudioListener(&engine);
    }

    AudioSource *MinAudioEngine::CreateSourceImpl()
    {
        return new MinAudioSource(&engine);
    }

    AudioClip *MinAudioEngine::CreateClipImpl(const AudioClipDesc &desc)
    {
        return new MinAudioClip(&engine, desc);
    }

} // namespace sky
