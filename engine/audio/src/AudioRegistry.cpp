//
// Created on 2026/09/21.
//

#include <audio/AudioRegistry.h>
#include <audio/AudioClip.h>
#include <audio/AudioClipAsset.h>
#include <audio/AudioEngine.h>
#include <audio/AudioSource.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    AudioRegistry::AudioRegistry() = default;

    AudioRegistry::~AudioRegistry() = default;

    void AudioRegistry::Reflect(SerializationContext *context)
    {
        context->Register<AudioLoadMode>("AudioLoadMode")
            .Enum(AudioLoadMode::InMemory, "InMemory")
            .Enum(AudioLoadMode::Streaming, "Streaming");

        context->Register<AttenuationModel>("AttenuationModel")
            .Enum(AttenuationModel::None, "None")
            .Enum(AttenuationModel::Inverse, "Inverse")
            .Enum(AttenuationModel::Linear, "Linear")
            .Enum(AttenuationModel::Exponential, "Exponential");

        context->Register<AudioClipDesc>("AudioClipDesc")
            .Member<&AudioClipDesc::source>("source")
            .Member<&AudioClipDesc::loadMode>("loadMode")
            .Member<&AudioClipDesc::defaultBus>("defaultBus")
            .Member<&AudioClipDesc::loop>("loop")
            .Member<&AudioClipDesc::duration>("duration")
            .Member<&AudioClipDesc::channels>("channels")
            .Member<&AudioClipDesc::sampleRate>("sampleRate");

        context->Register<AudioClipData>("AudioClipData")
            .Member<&AudioClipData::desc>("desc")
            .BinLoad<&AudioClipData::Load>()
            .BinSave<&AudioClipData::Save>();
    }

    bool AudioRegistry::CreateEngine()
    {
        if (factory == nullptr) {
            return false;
        }

        if (engine != nullptr) {
            return true;
        }

        engine.reset(factory->CreateAudioEngine());
        if (engine == nullptr) {
            return false;
        }

        if (!engine->Init()) {
            engine.reset();
            return false;
        }

        return true;
    }

    void AudioRegistry::DestroyEngine()
    {
        if (engine != nullptr) {
            engine->Shutdown();
            engine.reset();
        }
    }

    AudioSource *AudioRegistry::CreateSource()
    {
        return engine != nullptr ? engine->CreateSource() : nullptr;
    }

    AudioClip *AudioRegistry::CreateClip(const AudioClipDesc &desc)
    {
        return engine != nullptr ? engine->CreateClip(desc) : nullptr;
    }

    void AudioRegistry::Register(Impl *impl)
    {
        factory.reset(impl);
    }

    void AudioRegistry::UnRegister()
    {
        DestroyEngine();
        factory.reset();
    }

} // namespace sky
