//
// Created by blues on 2024/11/8.
//

#pragma once

#include <framework/world/Component.h>
#include <framework/serialization/ArrayVisitor.h>
#include <core/util/Uuid.h>
#include <terrain/TerrainBase.h>

namespace sky {
    class SerializationContext;
    class TerrainFeatureProcessor;

    struct TerrainData {
        ClipmapConfig config;

        Uuid material;

        uint32_t tileCountX = 0;
        uint32_t tileCountY = 0;
        std::vector<Uuid> heightmapTiles;
        std::vector<Uuid> splatmapTiles;
        std::vector<LayerInfo> layers;
    };

    class TerrainComponent : public ComponentAdaptor<TerrainData> {
    public:
        TerrainComponent() = default;
        ~TerrainComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(TerrainComponent)

        void Tick(float time) override;

        void UpdateHeightMap(std::vector<Uuid> &&tiles);

    private:
        void OnSerialized() override;
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        TerrainFeatureProcessor *featureProcessor = nullptr;
    };

} // namespace sky




