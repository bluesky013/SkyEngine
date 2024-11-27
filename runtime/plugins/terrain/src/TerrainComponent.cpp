//
// Created by blues on 2024/11/21.
//

#include <terrain/TerrainComponent.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    void TerrainComponent::Reflect(SerializationContext *context)
    {
        context->Register<TerrainCoord>("TerrainCoord")
            .Member<&TerrainCoord::x>("x")
            .Member<&TerrainCoord::y>("y");

        context->Register<TerrainSectionData>("TerrainSectionData")
            .Member<&TerrainSectionData::coord>("coord")
            .Member<&TerrainSectionData::heightMap>("heightMap");
        
        context->Register<TerrainSectionSize>("SectionSize")
            .Enum(TerrainSectionSize::S31x31, "31x31")
            .Enum(TerrainSectionSize::S63x63, "63x63")
            .Enum(TerrainSectionSize::S127x127, "127x127");

        context->Register<TerrainData>("TerrainData")
            .Member<&TerrainData::resolution>("resolution")
            .Member<&TerrainData::sectionSize>("sectionSize")
            .Member<&TerrainData::sections>("sections");

        REGISTER_BEGIN(TerrainComponent, context)
            REGISTER_MEMBER_NS(sections, Sections, SectionChanged)
            REGISTER_MEMBER(size, SetSize, GetSize)
            REGISTER_MEMBER(resolution, SetResolution, GetResolution);
            
    }

    void TerrainComponent::Tick(float time)
    {

    }

    SequenceVisitor TerrainComponent::Sections()
    {
        const auto *info = TypeInfoObj<std::vector<TerrainSectionData>>::Get()->RtInfo();
        SKY_ASSERT(info != nullptr);

        return {info->containerInfo, reinterpret_cast<void *>(&data.sections)};
    }

    void TerrainComponent::SectionChanged()
    {

    }

    void TerrainComponent::SetSize(const TerrainSectionSize& size)
    {
        data.sectionSize = size;
        OnRebuildTerrain();
    }

    const TerrainSectionSize& TerrainComponent::GetSize() const
    {
        return data.sectionSize;
    }

    void TerrainComponent::SetResolution(float resolution)
    {
        data.resolution = resolution;
        OnRebuildTerrain();
    }

    float TerrainComponent::GetResolution() const
    {
        return data.resolution;
    }

    void TerrainComponent::OnRebuildTerrain()
    {

    }

} // namespace sky