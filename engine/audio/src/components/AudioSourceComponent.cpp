//
// Created on 2026/09/21.
//

#include <audio/components/AudioSourceComponent.h>
#include <audio/AudioClip.h>
#include <audio/AudioClipAsset.h>
#include <audio/AudioEngine.h>
#include <audio/AudioRegistry.h>
#include <audio/AudioSource.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/Actor.h>
#include <framework/world/ComponentFactory.h>
#include <framework/world/TransformComponent.h>

namespace sky {

    AudioSourceComponent::AudioSourceComponent() = default;

    AudioSourceComponent::~AudioSourceComponent() = default;

    void AudioSourceComponent::Reflect(SerializationContext *context)
    {
        context->Register<AudioSourceData>("AudioSourceData")
            .Member<&AudioSourceData::clip>("clip")
            .Member<&AudioSourceData::volume>("volume")
            .Member<&AudioSourceData::pitch>("pitch")
            .Member<&AudioSourceData::loop>("loop")
            .Member<&AudioSourceData::playOnStart>("playOnStart")
            .Member<&AudioSourceData::spatialBlend>("spatialBlend")
            .Member<&AudioSourceData::minDistance>("minDistance")
            .Member<&AudioSourceData::maxDistance>("maxDistance")
            .Member<&AudioSourceData::bus>("bus");

        REGISTER_BEGIN(AudioSourceComponent, context)
            REGISTER_MEMBER(clip, SetClip, GetClip)
                SET_ASSET_TYPE(std::string_view("AudioClip"))
            REGISTER_MEMBER(volume, SetVolume, GetVolume)
            REGISTER_MEMBER(pitch, SetPitch, GetPitch)
            REGISTER_MEMBER(loop, SetLoop, GetLoop)
            REGISTER_MEMBER(playOnStart, SetPlayOnStart, GetPlayOnStart)
            REGISTER_MEMBER(spatialBlend, SetSpatialBlend, GetSpatialBlend)
            REGISTER_MEMBER(minDistance, SetMinDistance, GetMinDistance)
            REGISTER_MEMBER(maxDistance, SetMaxDistance, GetMaxDistance)
            REGISTER_MEMBER(bus, SetBus, GetBus);

        ComponentFactory::Get()->RegisterComponent<AudioSourceComponent>("Audio");
    }

    void AudioSourceComponent::SetClip(const Uuid &id)
    {
        data.clip = id;
        LoadClipResource();
    }

    void AudioSourceComponent::SetVolume(float value)
    {
        data.volume = value;
        if (source != nullptr) {
            source->SetVolume(value);
        }
    }

    void AudioSourceComponent::SetPitch(float value)
    {
        data.pitch = value;
        if (source != nullptr) {
            source->SetPitch(value);
        }
    }

    void AudioSourceComponent::SetLoop(bool value)
    {
        data.loop = value;
        if (source != nullptr) {
            source->SetLoop(value);
        }
    }

    void AudioSourceComponent::SetPlayOnStart(bool value)
    {
        data.playOnStart = value;
    }

    void AudioSourceComponent::SetSpatialBlend(float value)
    {
        data.spatialBlend = value;
        if (source != nullptr) {
            source->SetSpatialBlend(value);
        }
    }

    void AudioSourceComponent::SetMinDistance(float value)
    {
        data.minDistance = value;
        if (source != nullptr) {
            source->SetAttenuation(AttenuationModel::Inverse, data.minDistance, data.maxDistance);
        }
    }

    void AudioSourceComponent::SetMaxDistance(float value)
    {
        data.maxDistance = value;
        if (source != nullptr) {
            source->SetAttenuation(AttenuationModel::Inverse, data.minDistance, data.maxDistance);
        }
    }

    void AudioSourceComponent::SetBus(const std::string &value)
    {
        data.bus = value;
        if (source == nullptr) {
            return;
        }

        auto *engine = AudioRegistry::Get()->GetEngine();
        source->SetBus(engine != nullptr ? engine->GetBus(value) : nullptr);
    }

    void AudioSourceComponent::ApplySourceParams()
    {
        auto *engine = AudioRegistry::Get()->GetEngine();
        if (source == nullptr || engine == nullptr) {
            return;
        }

        source->SetVolume(data.volume);
        source->SetPitch(data.pitch);
        source->SetLoop(data.loop);
        source->SetSpatialBlend(data.spatialBlend);
        source->SetAttenuation(AttenuationModel::Inverse, data.minDistance, data.maxDistance);
        source->SetBus(engine->GetBus(data.bus));
    }

    void AudioSourceComponent::LoadClipResource()
    {
        if (!data.clip) {
            return;
        }

        auto asset = AssetManager::Get()->LoadAsset<AudioClip>(data.clip);
        if (asset == nullptr) {
            return;
        }

        asset->BlockUntilLoaded();
        clip = CreateAudioClipFromAsset(asset);

        if (source != nullptr && clip) {
            source->SetClip(clip.Get());
        }
    }

    void AudioSourceComponent::SyncTransform()
    {
        if (source == nullptr || actor == nullptr) {
            return;
        }

        auto *transform = actor->GetComponent<TransformComponent>();
        if (transform == nullptr) {
            return;
        }

        source->SetPosition(transform->GetWorldTransform().translation);
    }

    void AudioSourceComponent::OnAttachToWorld()
    {
        auto *engine = AudioRegistry::Get()->GetEngine();
        if (engine == nullptr) {
            return;
        }

        source = engine->CreateSource();
        if (source == nullptr) {
            return;
        }

        ApplySourceParams();
        LoadClipResource();
        SyncTransform();

        if (data.playOnStart) {
            source->Play();
        }
    }

    void AudioSourceComponent::OnDetachFromWorld()
    {
        if (source != nullptr) {
            source->Stop();
            delete source;
            source = nullptr;
        }
        clip = nullptr;
    }

    void AudioSourceComponent::Tick(float time)
    {
        // Transform sync is driven by AudioSystem so every source is updated once per world tick.
    }

} // namespace sky
