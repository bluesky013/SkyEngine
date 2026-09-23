//
// Created on 2026/09/22.
//

#pragma once

#include <framework/world/Component.h>

#include <terrain/TerrainTypes.h>

#include <core/util/Uuid.h>

namespace sky {
    class SerializationContext;
} // namespace sky

namespace sky::terrain {

    class TerrainSystem;

    // Logic-only terrain component data: layout metadata, asset/source references, streaming and
    // generation parameters. No render types.
    struct TerrainComponentData {
        // Layout (mirrors TerrainMeta).
        uint32_t tileSize     = 64;
        float    resolution   = 1.f;
        uint32_t heightFormat = 0;   // TerrainHeightFormat
        float    heightScale  = 256.f;
        float    heightOffset = 0.f;
        uint32_t tileCountX   = 0;
        uint32_t tileCountY   = 0;
        uint32_t layerCount   = 0;
        uint32_t lodCount     = 1;
        float    originX = 0.f;
        float    originY = 0.f;
        float    originZ = 0.f;

        // References.
        Uuid terrainAsset;
        Uuid source;
        Uuid material;

        // Streaming.
        float    loadRadius   = 128.f;
        float    unloadRadius = 192.f;
        uint32_t loadBudget   = 8;

        // Generation.
        uint32_t generateSeed      = 0;
        uint32_t generationEnabled = 0;

        TerrainMeta ToMeta() const;

        static void Reflect(SerializationContext *context);
    };

    class TerrainComponent : public ComponentAdaptor<TerrainComponentData> {
    public:
        TerrainComponent()           = default;
        ~TerrainComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(TerrainComponent)

    private:
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
        void Tick(float time) override;

        TerrainSystem *GetSystem() const;
    };

} // namespace sky::terrain
