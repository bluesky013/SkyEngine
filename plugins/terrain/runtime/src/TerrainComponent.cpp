//
// Created by blues on 2024/11/21.
//

#include <terrain/TerrainComponent.h>
#include <terrain/TerrainFeatureProcessor.h>
#include <framework/serialization/SerializationContext.h>
#include <render/adaptor/Util.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/ImageAsset.h>

namespace sky {

    void TerrainComponent::Reflect(SerializationContext *context)
    {
        context->Register<TerrainHeightFormat>("TerrainHeightFormat")
            .Enum(TerrainHeightFormat::R16_UNORM, "R16_UNORM")
            .Enum(TerrainHeightFormat::R32_SFLOAT, "R32_SFLOAT");

        context->Register<ClipmapConfig>("ClipmapConfig")
            .Member<&ClipmapConfig::blockSize>("blockSize")
            .Member<&ClipmapConfig::numLevels>("numLevels")
            .Member<&ClipmapConfig::resolution>("resolution")
            .Member<&ClipmapConfig::heightScale>("heightScale")
            .Member<&ClipmapConfig::heightOffset>("heightOffset")
            .Member<&ClipmapConfig::heightFormat>("heightFormat");

        context->Register<LayerInfo>("LayerInfo")
            .Member<&LayerInfo::name>("name")
            .Member<&LayerInfo::albedo>("albedo")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Texture>::ASSET_TYPE))
            .Member<&LayerInfo::normal>("normal")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Texture>::ASSET_TYPE))
            .Member<&LayerInfo::roughness>("roughness")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Texture>::ASSET_TYPE));

        context->Register<TerrainGenerateConfig>("TerrainGenerateConfig")
            .Member<&TerrainGenerateConfig::seed>("Seed");

        context->Register<TerrainData>("TerrainData")
            .Member<&TerrainData::config>("config")
            .Member<&TerrainData::material>("material")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<MaterialInstance>::ASSET_TYPE))
            .Member<&TerrainData::tileCountX>("tileCountX")
            .Member<&TerrainData::tileCountY>("tileCountY")
            .Member<&TerrainData::heightmapTiles>("heightmapTiles")
            .Member<&TerrainData::splatmapTiles>("splatmapTiles")
            .Member<&TerrainData::layers>("layers");

        REGISTER_BEGIN(TerrainComponent, context);
    }

    void TerrainComponent::Tick(float time)
    {
    }

    void TerrainComponent::OnSerialized()
    {
        // Data deserialized — will be pushed to FP when attached to world
    }

    void TerrainComponent::OnAttachToWorld()
    {
        featureProcessor = GetFeatureProcessor<TerrainFeatureProcessor>(actor);
        if (featureProcessor != nullptr) {
            featureProcessor->SetConfig(data.config);
            featureProcessor->SetMaterial(data.material);
            featureProcessor->SetTileData(data.tileCountX, data.tileCountY,
                                          data.heightmapTiles, data.splatmapTiles, data.layers);
        }
    }

    void TerrainComponent::OnDetachFromWorld()
    {
        if (featureProcessor != nullptr) {
            featureProcessor->Reset();
            featureProcessor = nullptr;
        }
    }

    void TerrainComponent::UpdateHeightMap(std::vector<Uuid> &&tiles)
    {
        data.heightmapTiles = std::move(tiles);
        if (featureProcessor != nullptr) {
            featureProcessor->SetTileData(data.tileCountX, data.tileCountY,
                                          data.heightmapTiles, data.splatmapTiles, data.layers);
        }
    }

} // namespace sky