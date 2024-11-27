//
// Created by blues on 2024/11/21.
//

#include <terrain/TerrainComponent.h>
#include <framework/serialization/SerializationContext.h>
#include <render/adaptor/assets/ImageAsset.h>

namespace sky {

    void TerrainComponent::Reflect(SerializationContext *context)
    {
        context->Register<TerrainCoord>("TerrainCoord")
            .Member<&TerrainCoord::x>("x")
            .Member<&TerrainCoord::y>("y");

        context->Register<TerrainSectionData>("TerrainSectionData")
            .Member<&TerrainSectionData::x>("x")
            .Member<&TerrainSectionData::y>("y")
            .Member<&TerrainSectionData::heightMap>("heightMap")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Texture>::ASSET_TYPE));
        
        context->Register<TerrainSectionSize>("SectionSize")
            .Enum(TerrainSectionSize::S31x31, "31x31")
            .Enum(TerrainSectionSize::S63x63, "63x63")
            .Enum(TerrainSectionSize::S127x127, "127x127");

        context->Register<TerrainQuad>("TerrainQuad")
            .Member<&TerrainQuad::resolution>("resolution")
            .Member<&TerrainQuad::sectionSize>("sectionSize");

        context->Register<TerrainBuildConfig>("TerrainBuildConfig")
            .Member<&TerrainBuildConfig::resolution>("Resolution")
            .Member<&TerrainBuildConfig::sectionSize>("Section Size")
            .Member<&TerrainBuildConfig::sectionNumX>("Section Num X")
            .Member<&TerrainBuildConfig::sectionNumY>("Section Num Y");

        context->Register<TerrainGenerateConfig>("TerrainGenerateConfig")
            .Member<&TerrainGenerateConfig::seed>("Seed");

        context->Register<TerrainData>("TerrainData")
            .Member<&TerrainData::resolution>("resolution")
            .Member<&TerrainData::sectionSize>("sectionSize")
            .Member<&TerrainData::sections>("sections");

        REGISTER_BEGIN(TerrainComponent, context);
    }

    void TerrainComponent::BuildTerrain(const TerrainBuildConfig &config)
    {
        data.sectionSize = config.sectionSize;
        data.resolution = config.resolution;
        data.sectionBoundX = config.sectionNumX;
        data.sectionBoundY = config.sectionNumY;
        for (int32_t i = 0; i < config.sectionNumX; ++i) {
            for (int32_t j = 0; j < config.sectionNumY; ++j) {
                AddSection(i, j);
            }
        }
    }

    void TerrainComponent::AddSection(int32_t x, int32_t y)
    {
        auto iter = std::find_if(data.sections.begin(), data.sections.end(),
            [x, y](const TerrainSectionData &data) -> bool {
            return x == data.x && y == data.y;
        });
        if (iter == data.sections.end()) {
            data.sections.emplace_back(TerrainSectionData{x, y}); // NOLINT
        }
    }

    void TerrainComponent::RemoveSection(int32_t x, int32_t y)
    {
        auto iter = std::find_if(data.sections.begin(), data.sections.end(),
            [x, y](const TerrainSectionData &data) -> bool {
                return x == data.x && y == data.y;
        });
        if (iter != data.sections.end()) {
            data.sections.erase(iter);
        }
    }

    void TerrainComponent::Tick(float time)
    {

    }

    void TerrainComponent::OnRebuildTerrain()
    {

    }

} // namespace sky