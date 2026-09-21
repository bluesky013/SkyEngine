//
// Created on 2026/09/21.
//

#include <audio/components/AudioListenerComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/ComponentFactory.h>

namespace sky {

    void AudioListenerComponent::Reflect(SerializationContext *context)
    {
        context->Register<AudioListenerData>("AudioListenerData")
            .Member<&AudioListenerData::active>("active");

        REGISTER_BEGIN(AudioListenerComponent, context)
            REGISTER_MEMBER(active, SetActive, IsActive);

        ComponentFactory::Get()->RegisterComponent<AudioListenerComponent>("Audio");
    }

} // namespace sky
