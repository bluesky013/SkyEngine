//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioTypes.h>
#include <core/environment/Singleton.h>

#include <memory>

namespace sky {

    class SerializationContext;
    class AudioClip;
    class AudioEngine;
    class AudioSource;

    class AudioRegistry : public Singleton<AudioRegistry> {
    public:
        AudioRegistry();
        ~AudioRegistry() override;

        static void Reflect(SerializationContext *context);

        class Impl {
        public:
            Impl()           = default;
            virtual ~Impl()  = default;

            virtual AudioEngine *CreateAudioEngine() = 0;
        };

        bool CreateEngine();
        void DestroyEngine();

        AudioEngine *GetEngine() const { return engine.get(); }
        bool HasBackend() const { return factory != nullptr; }

        AudioSource *CreateSource();
        AudioClip *CreateClip(const AudioClipDesc &desc);

        void Register(Impl *impl);
        void UnRegister();

    private:
        std::unique_ptr<Impl>        factory;
        std::unique_ptr<AudioEngine> engine;
    };

} // namespace sky
