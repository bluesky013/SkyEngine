//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioTypes.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace sky {

    class AudioBus;
    class AudioClip;
    class AudioListener;
    class AudioSource;

    class AudioEngine {
    public:
        AudioEngine();
        virtual ~AudioEngine();

        AudioEngine(const AudioEngine &)            = delete;
        AudioEngine &operator=(const AudioEngine &) = delete;

        bool Init();
        void Shutdown();
        bool IsValid() const { return valid; }

        AudioBus *GetMasterBus() const { return masterBus.get(); }
        AudioBus *GetBus(const std::string &busName) const;
        AudioBus *CreateBus(const std::string &busName, AudioBus *parent);

        AudioListener *GetListener() const { return listener.get(); }

        AudioSource *CreateSource();
        AudioClip *CreateClip(const AudioClipDesc &desc);

    protected:
        virtual bool OnInitDevice()          = 0;
        virtual void OnShutdownDevice()      = 0;
        virtual AudioBus *CreateBusImpl(const std::string &busName) = 0;
        virtual AudioListener *CreateListenerImpl()                 = 0;
        virtual AudioSource *CreateSourceImpl()                     = 0;
        virtual AudioClip *CreateClipImpl(const AudioClipDesc &desc) = 0;

    private:
        bool valid = false;

        std::unique_ptr<AudioBus>                                   masterBus;
        std::unique_ptr<AudioListener>                              listener;
        std::unordered_map<std::string, std::unique_ptr<AudioBus>>  buses;
    };

} // namespace sky
