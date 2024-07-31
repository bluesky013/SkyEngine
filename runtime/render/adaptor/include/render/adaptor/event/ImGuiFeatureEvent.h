//
// Created by blues on 2024/7/30.
//

#pragma once

#include <core/event/Event.h>
#include <render/resource/Technique.h>

namespace sky {

    class ImGuiFeatureEvent : public EventTraits {
    public:
        ImGuiFeatureEvent() = default;
        virtual ~ImGuiFeatureEvent() = default;

        virtual void OnTechniqueReady(RDGfxTechPtr &tech) = 0;
    };

} // namespace sky
