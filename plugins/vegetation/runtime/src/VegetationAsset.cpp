//
// Created on 2026/09/22.
//

#include <vegetation/VegetationAsset.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::vegetation {

    namespace {

        void SaveRaw(BinaryOutputArchive &archive, const std::vector<uint8_t> &data)
        {
            const auto size = static_cast<uint32_t>(data.size());
            archive.SaveValue(size);
            if (size > 0) {
                archive.SaveValue(reinterpret_cast<const char *>(data.data()), size);
            }
        }

        void LoadRaw(BinaryInputArchive &archive, std::vector<uint8_t> &data)
        {
            uint32_t size = 0;
            archive.LoadValue(size);
            data.resize(size);
            if (size > 0) {
                archive.LoadValue(reinterpret_cast<char *>(data.data()), size);
            }
        }

        void SaveVec3(BinaryOutputArchive &archive, const Vector3 &v)
        {
            archive.SaveValue(v.x);
            archive.SaveValue(v.y);
            archive.SaveValue(v.z);
        }

        void LoadVec3(BinaryInputArchive &archive, Vector3 &v)
        {
            archive.LoadValue(v.x);
            archive.LoadValue(v.y);
            archive.LoadValue(v.z);
        }

    } // namespace

    void VegetationAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(config.seed);
        archive.SaveValue(config.pointsPerSquareMeter);

        archive.SaveValue(static_cast<uint32_t>(palette.biomes.size()));
        for (const auto &biome : palette.biomes) {
            archive.SaveValue(biome.id);
            archive.SaveValue(biome.minSlopeDeg);
            archive.SaveValue(biome.maxSlopeDeg);
            archive.SaveValue(biome.minHeight);
            archive.SaveValue(biome.maxHeight);
            archive.SaveValue(biome.layerIndex);
            archive.SaveValue(biome.minLayerWeight);
            archive.SaveValue(biome.density);

            archive.SaveValue(static_cast<uint32_t>(biome.species.size()));
            for (const auto &species : biome.species) {
                archive.SaveValue(species.mesh.ToString());
                archive.SaveValue(species.density);
                archive.SaveValue(species.scaleMin);
                archive.SaveValue(species.scaleMax);
                archive.SaveValue(species.rotationJitter);
                archive.SaveValue(species.windResponse);
            }
        }

        archive.SaveValue(static_cast<uint32_t>(densityMaps.size()));
        for (const auto &map : densityMaps) {
            SaveVec3(archive, map.origin);
            archive.SaveValue(map.resolution);
            archive.SaveValue(map.sizeX);
            archive.SaveValue(map.sizeY);
            SaveRaw(archive, map.data);
        }
    }

    void VegetationAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(config.seed);
        archive.LoadValue(config.pointsPerSquareMeter);

        uint32_t biomeCount = 0;
        archive.LoadValue(biomeCount);
        palette.biomes.resize(biomeCount);
        for (auto &biome : palette.biomes) {
            archive.LoadValue(biome.id);
            archive.LoadValue(biome.minSlopeDeg);
            archive.LoadValue(biome.maxSlopeDeg);
            archive.LoadValue(biome.minHeight);
            archive.LoadValue(biome.maxHeight);
            archive.LoadValue(biome.layerIndex);
            archive.LoadValue(biome.minLayerWeight);
            archive.LoadValue(biome.density);

            uint32_t speciesCount = 0;
            archive.LoadValue(speciesCount);
            biome.species.resize(speciesCount);
            for (auto &species : biome.species) {
                std::string meshStr;
                archive.LoadValue(meshStr);
                species.mesh = Uuid::CreateFromString(meshStr);
                archive.LoadValue(species.density);
                archive.LoadValue(species.scaleMin);
                archive.LoadValue(species.scaleMax);
                archive.LoadValue(species.rotationJitter);
                archive.LoadValue(species.windResponse);
            }
        }

        uint32_t mapCount = 0;
        archive.LoadValue(mapCount);
        densityMaps.resize(mapCount);
        for (auto &map : densityMaps) {
            LoadVec3(archive, map.origin);
            archive.LoadValue(map.resolution);
            archive.LoadValue(map.sizeX);
            archive.LoadValue(map.sizeY);
            LoadRaw(archive, map.data);
        }
    }

    void VegetationAssetData::Reflect(SerializationContext *context)
    {
        context->Register<VegetationAssetData>("VegetationAssetData")
            .BinLoad<&VegetationAssetData::Load>()
            .BinSave<&VegetationAssetData::Save>();
    }

    void VegetationSourceData::Reflect(SerializationContext *context)
    {
        context->Register<VegetationSourceData>("VegetationSourceData")
            .Member<&VegetationSourceData::seed>("seed")
            .Member<&VegetationSourceData::pointsPerSquareMeter>("pointsPerSquareMeter")
            .Member<&VegetationSourceData::paletteSource>("paletteSource")
            .Member<&VegetationSourceData::biomeCount>("biomeCount");
    }

    void RegisterVegetationAssetType()
    {
        VegetationAssetData::Reflect(SerializationContext::Get());
        VegetationSourceData::Reflect(SerializationContext::Get());
        AssetManager::Get()->RegisterAssetHandler<VegetationAsset>();
    }

} // namespace sky::vegetation
