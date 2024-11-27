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

    struct TerrainSectionData {
        int32_t x;
        int32_t y;
        Uuid heightMap;
    };

    struct TerrainData {
        TerrainSectionSize sectionSize;
        int32_t sectionBoundX = 8;
        int32_t sectionBoundY = 8;
        float resolution = 1.f;
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
    private:
        void OnRebuildTerrain();
    };

} // namespace sky




