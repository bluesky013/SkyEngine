//
// Created on 2026/09/21.
//

#include "MinAudioBackend.h"

#include <audio/AudioClipAsset.h>
#include <audio/AudioRegistry.h>
#include <audio/components/AudioListenerComponent.h>
#include <audio/components/AudioSourceComponent.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/IModule.h>
#include <framework/serialization/SerializationContext.h>

#include <core/logger/Logger.h>

static const char *TAG = "AudioModule";

namespace sky {

    class MinAudioFactory : public AudioRegistry::Impl {
    public:
        MinAudioFactory()           = default;
        ~MinAudioFactory() override = default;

        AudioEngine *CreateAudioEngine() override
        {
            return new MinAudioEngine();
        }
    };

    class AudioModule : public IModule {
    public:
        AudioModule()           = default;
        ~AudioModule() override = default;

        bool Init(const StartArguments &args) override
        {
            return true;
        }

        void Start() override
        {
            AudioRegistry::Get()->Register(new MinAudioFactory());
            if (!AudioRegistry::Get()->CreateEngine()) {
                LOG_W(TAG, "Audio engine unavailable, audio runs in silent mode.");
            }

            auto *context = SerializationContext::Get();
            AudioRegistry::Reflect(context);
            AudioSourceComponent::Reflect(context);
            AudioListenerComponent::Reflect(context);

            AssetManager::Get()->RegisterAssetHandler<AudioClip>();
        }

        void Shutdown() override
        {
            AudioRegistry::Get()->UnRegister();
        }
    };

} // namespace sky
REGISTER_MODULE(sky::AudioModule)
