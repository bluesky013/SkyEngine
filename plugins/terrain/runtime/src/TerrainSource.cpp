//
// Created on 2026/09/22.
//

#include <terrain/TerrainSource.h>

#include <framework/serialization/SerializationContext.h>

namespace sky::terrain {

    void TerrainSourceData::Reflect(SerializationContext *context)
    {
        context->Register<TerrainSourceData>("TerrainSourceData")
            .Member<&TerrainSourceData::tileSize>("tileSize")
            .Member<&TerrainSourceData::resolution>("resolution")
            .Member<&TerrainSourceData::heightFormat>("heightFormat")
            .Member<&TerrainSourceData::decodeScale>("decodeScale")
            .Member<&TerrainSourceData::decodeOffset>("decodeOffset")
            .Member<&TerrainSourceData::tileCountX>("tileCountX")
            .Member<&TerrainSourceData::tileCountY>("tileCountY")
            .Member<&TerrainSourceData::tileStartX>("tileStartX")
            .Member<&TerrainSourceData::tileStartY>("tileStartY")
            .Member<&TerrainSourceData::originX>("originX")
            .Member<&TerrainSourceData::originY>("originY")
            .Member<&TerrainSourceData::originZ>("originZ")
            .Member<&TerrainSourceData::seed>("seed")
            .Member<&TerrainSourceData::baseFrequency>("baseFrequency")
            .Member<&TerrainSourceData::octaves>("octaves")
            .Member<&TerrainSourceData::lacunarity>("lacunarity")
            .Member<&TerrainSourceData::gain>("gain")
            .Member<&TerrainSourceData::warpStrength>("warpStrength")
            .Member<&TerrainSourceData::warpFrequency>("warpFrequency")
            .Member<&TerrainSourceData::ridgedStrength>("ridgedStrength")
            .Member<&TerrainSourceData::heightScale>("heightScale")
            .Member<&TerrainSourceData::heightOffset>("heightOffset")
            .Member<&TerrainSourceData::lodCount>("lodCount")
            .Member<&TerrainSourceData::layerCount>("layerCount");
    }

    void ToGenerateConfig(const TerrainSourceData &source, TerrainMeta &outMeta, TerrainGenerateConfig &outConfig)
    {
        outMeta.tileSize     = source.tileSize;
        outMeta.resolution   = source.resolution;
        outMeta.heightFormat = static_cast<TerrainHeightFormat>(source.heightFormat);
        outMeta.heightScale  = source.decodeScale;
        outMeta.heightOffset = source.decodeOffset;
        outMeta.tileCountX   = source.tileCountX;
        outMeta.tileCountY   = source.tileCountY;
        outMeta.layerCount   = source.layerCount;
        outMeta.lodCount     = source.lodCount > 0 ? source.lodCount : 1u;
        outMeta.origin       = Vector3(source.originX, source.originY, source.originZ);

        outConfig.seed = source.seed;
        outConfig.noise.baseFrequency  = source.baseFrequency;
        outConfig.noise.octaves        = static_cast<int>(source.octaves);
        outConfig.noise.lacunarity     = source.lacunarity;
        outConfig.noise.gain           = source.gain;
        outConfig.noise.warpStrength   = source.warpStrength;
        outConfig.noise.warpFrequency  = source.warpFrequency;
        outConfig.noise.ridgedStrength = source.ridgedStrength;
        outConfig.heightScale  = source.heightScale;
        outConfig.heightOffset = source.heightOffset;
        outConfig.layerCount   = source.layerCount;
        outConfig.lodCount     = source.lodCount > 0 ? source.lodCount : 1u;
    }

} // namespace sky::terrain
