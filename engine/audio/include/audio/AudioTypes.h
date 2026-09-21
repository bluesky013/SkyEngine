//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky {

    enum class AudioLoadMode : uint32_t {
        InMemory = 0,
        Streaming = 1
    };

    enum class AttenuationModel : uint32_t {
        None = 0,
        Inverse,
        Linear,
        Exponential
    };

    namespace audio {
        inline constexpr const char *MASTER_BUS = "master";
        inline constexpr const char *MUSIC_BUS  = "music";
        inline constexpr const char *SFX_BUS    = "sfx";
    } // namespace audio

    struct AudioClipDesc {
        std::string    source;
        AudioLoadMode  loadMode   = AudioLoadMode::InMemory;
        std::string    defaultBus = audio::SFX_BUS;
        bool           loop       = false;
        float          duration   = 0.f;
        uint32_t       channels   = 0;
        uint32_t       sampleRate = 0;
    };

    struct AudioPlaybackDesc {
        float            volume        = 1.f;
        float            pitch         = 1.f;
        bool             loop          = false;
        float            spatialBlend  = 0.f;
        float            minDistance   = 1.f;
        float            maxDistance   = 50.f;
        AttenuationModel attenuation   = AttenuationModel::Inverse;
        float            dopplerFactor = 0.f;
    };

} // namespace sky
