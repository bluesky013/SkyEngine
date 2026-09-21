//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioTypes.h>
#include <core/template/ReferenceObject.h>

#include <cstdint>
#include <vector>

namespace sky {

    class AudioClip : public RefObject {
    public:
        AudioClip()          = default;
        ~AudioClip() override = default;

        void SetDesc(const AudioClipDesc &inDesc) { desc = inDesc; }
        const AudioClipDesc &GetDesc() const { return desc; }

        // Encoded source bytes baked into the asset, so packaged builds do not depend on loose files.
        void SetEncodedData(std::vector<uint8_t> data) { encodedData = std::move(data); }
        const std::vector<uint8_t> &GetEncodedData() const { return encodedData; }
        bool HasEncodedData() const { return !encodedData.empty(); }

        float GetDuration() const { return desc.duration; }
        uint32_t GetChannels() const { return desc.channels; }
        uint32_t GetSampleRate() const { return desc.sampleRate; }
        AudioLoadMode GetLoadMode() const { return desc.loadMode; }
        const std::string &GetDefaultBus() const { return desc.defaultBus; }
        bool IsLoop() const { return desc.loop; }

        virtual bool Load() = 0;
        virtual void Unload() = 0;
        virtual bool IsLoaded() const = 0;

    protected:
        AudioClipDesc        desc;
        std::vector<uint8_t> encodedData;
    };

} // namespace sky
