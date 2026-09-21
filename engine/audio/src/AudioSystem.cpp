//
// Created on 2026/09/21.
//

#include <audio/AudioSystem.h>
#include <audio/AudioEngine.h>
#include <audio/AudioListener.h>
#include <audio/AudioRegistry.h>
#include <audio/AudioSource.h>
#include <audio/components/AudioListenerComponent.h>
#include <audio/components/AudioSourceComponent.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

namespace sky {

    AudioSystem::AudioSystem() = default;

    AudioSystem::~AudioSystem() = default;

    AudioSource *AudioSystem::Play(AudioClip *clip, const AudioPlaybackDesc &desc)
    {
        if (engine == nullptr) {
            return nullptr;
        }

        auto *source = engine->CreateSource();
        if (source == nullptr) {
            return nullptr;
        }

        source->SetClip(clip);
        source->SetVolume(desc.volume);
        source->SetPitch(desc.pitch);
        source->SetLoop(desc.loop);
        source->SetSpatialBlend(desc.spatialBlend);
        source->SetAttenuation(desc.attenuation, desc.minDistance, desc.maxDistance);
        source->SetDopplerFactor(desc.dopplerFactor);

        sources.emplace_back(source);
        source->Play();
        return source;
    }

    void AudioSystem::OnAttachToWorld(World &inWorld)
    {
        world  = &inWorld;
        engine = AudioRegistry::Get()->GetEngine();
    }

    void AudioSystem::OnDetachFromWorld(World &inWorld)
    {
        sources.clear();
        engine = nullptr;
        world  = nullptr;
    }

    void AudioSystem::Tick(float time)
    {
        if (world == nullptr) {
            return;
        }

        UpdateListener(*world);

        for (const auto &actor : world->GetActors()) {
            if (auto *component = actor->GetComponent<AudioSourceComponent>(); component != nullptr) {
                component->SyncTransform();
            }
        }
    }

    void AudioSystem::UpdateListener(World &inWorld)
    {
        if (engine == nullptr) {
            return;
        }

        auto *listener = engine->GetListener();
        if (listener == nullptr) {
            return;
        }

        for (const auto &actor : inWorld.GetActors()) {
            auto *component = actor->GetComponent<AudioListenerComponent>();
            if (component == nullptr || !component->IsActive()) {
                continue;
            }

            auto *transform = actor->GetComponent<TransformComponent>();
            if (transform == nullptr) {
                continue;
            }

            const auto &worldTrans = transform->GetWorldTransform();
            listener->SetPosition(worldTrans.translation);
            listener->SetOrientation(worldTrans.rotation * Vector3(0.f, 0.f, 1.f),
                                     worldTrans.rotation * Vector3(0.f, 1.f, 0.f));
            return;
        }
    }

} // namespace sky
