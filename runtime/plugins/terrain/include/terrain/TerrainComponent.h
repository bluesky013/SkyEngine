//
// Created by blues on 2024/11/8.
//

#pragma once

#include <framework/world/Component.h>
#include <core/util/Uuid.h>

namespace sky {
    class SerializationContext;

    struct TerrainSection {

    };

    struct TerrainData {
        uint32_t sectionSize = 63;
        float resolution = 1.f;
    };

    class TerrainComponent : public ComponentAdaptor<TerrainData> {
    public:
        TerrainComponent() = default;
        ~TerrainComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(TerrainComponent)

        void Tick(float time) override;
    };

} // namespace sky




