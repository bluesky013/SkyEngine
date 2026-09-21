//
// Created on 2026/09/21.
//

#pragma once

#include <framework/world/Component.h>

namespace sky {

    class SerializationContext;

    struct AudioListenerData {
        bool active = true;
    };

    class AudioListenerComponent : public ComponentAdaptor<AudioListenerData> {
    public:
        AudioListenerComponent()           = default;
        ~AudioListenerComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(AudioListenerComponent)

        void SetActive(bool value) { data.active = value; }
        bool IsActive() const { return data.active; }
    };

} // namespace sky
