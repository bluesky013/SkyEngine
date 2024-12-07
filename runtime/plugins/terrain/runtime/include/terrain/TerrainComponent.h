//
// Created by blues on 2024/11/8.
//

#pragma once

#include <framework/world/Component.h>
#include <framework/serialization/ArrayVisitor.h>
#include <core/util/Uuid.h>
#include <terrain/TerrainBase.h>
#include <terrain/TerrainRender.h>

namespace sky {
    class RenderScene;
    class SerializationContext;

    struct TerrainSectionData {
        TerrainCoord coord;
        Uuid heightMap;
    };

    struct TerrainData {
        TerrainSectionSize sectionSize;
        float resolution = 1.f;
        int32_t sectionBoundX = 8;
        int32_t sectionBoundY = 8;

        Uuid material;
        std::vector<TerrainSectionData> sections;
    };

    class TerrainComponent : public ComponentAdaptor<TerrainData> {
    public:
        TerrainComponent() = default;
        ~TerrainComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(TerrainComponent)

        void Tick(float time) override;

        void BuildTerrain(const TerrainBuildConfig &config);
        void AddSection(int32_t x, int32_t y);
        void RemoveSection(int32_t x, int32_t y);

        void UpdateHeightMap(std::vector<TerrainSectionData> &&data);
    private:
        void LoadMaterial();
        void OnRebuildTerrain();
        void ResetRender(RenderScene*);

        void OnSerialized() override;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        RDMaterialInstancePtr material;
        std::unique_ptr<TerrainRender> terrainRender;
    };

} // namespace sky




