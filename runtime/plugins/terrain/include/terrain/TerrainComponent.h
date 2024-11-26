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

    struct TerrainSection {
        TerrainCoord coord;
    };

    struct TerrainData {
        TerrainSectionSize sectionSize;
        float resolution = 1.f;

        std::vector<TerrainSection> sections;
    };

    class TerrainComponent : public ComponentAdaptor<TerrainData> {
    public:
        TerrainComponent() = default;
        ~TerrainComponent() override = default;

        static void Reflect(SerializationContext *context);
        COMPONENT_RUNTIME_INFO(TerrainComponent)

        void Tick(float time) override;

        SequenceVisitor Sections();

        void SectionChanged();

        void SetSize(const TerrainSectionSize &size);
        const TerrainSectionSize &GetSize() const;

        void SetResolution(float resolution);
        float GetResolution() const;

    private:
        void OnRebuildTerrain();
    };

} // namespace sky




