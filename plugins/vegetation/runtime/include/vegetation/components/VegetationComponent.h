//
// Created on 2026/09/22.
//

#pragma once

#include <framework/world/Component.h>

#include <vegetation/VegetationTypes.h>

#include <core/util/Uuid.h>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::vegetation {

    class VegetationSystem;

    // Logic-only vegetation component data: asset/source references, seed, density/LOD and streaming
    // parameters. No render types.
    struct VegetationComponentData {
        Uuid     vegetationAsset;
        Uuid     source;
        uint32_t seed                 = 0;
        float    pointsPerSquareMeter = 1.f;
        float    loadRadius           = 64.f;
        float    unloadRadius         = 96.f;
        uint32_t loadBudget           = 8;

        static void Reflect(SerializationContext *context);
    };

    class VegetationComponent : public ComponentAdaptor<VegetationComponentData> {
    public:
        VegetationComponent()           = default;
        ~VegetationComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(VegetationComponent)

    private:
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
        void Tick(float time) override;

        VegetationSystem *GetSystem() const;
    };

} // namespace sky::vegetation
