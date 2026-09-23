//
// Created on 2026/09/22.
//

#include <terrain/TerrainAsset.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::terrain {

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

    void TerrainAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);

        archive.SaveValue(meta.tileSize);
        archive.SaveValue(meta.resolution);
        archive.SaveValue(meta.heightFormat);
        archive.SaveValue(meta.heightScale);
        archive.SaveValue(meta.heightOffset);
        archive.SaveValue(meta.tileCountX);
        archive.SaveValue(meta.tileCountY);
        archive.SaveValue(meta.layerCount);
        archive.SaveValue(meta.lodCount);
        SaveVec3(archive, meta.origin);

        archive.SaveValue(static_cast<uint32_t>(manifest.size()));
        for (const auto &info : manifest) {
            archive.SaveValue(info.coord.x);
            archive.SaveValue(info.coord.y);
            SaveVec3(archive, info.bounds.min);
            SaveVec3(archive, info.bounds.max);
            archive.SaveValue(info.minHeight);
            archive.SaveValue(info.maxHeight);
            archive.SaveValue(info.lodCount);
        }

        archive.SaveValue(static_cast<uint32_t>(tiles.size()));
        for (const auto &tile : tiles) {
            archive.SaveValue(tile.coord.x);
            archive.SaveValue(tile.coord.y);
            archive.SaveValue(static_cast<uint32_t>(tile.lods.size()));
            for (const auto &lod : tile.lods) {
                SaveRaw(archive, lod.height);
                archive.SaveValue(static_cast<uint32_t>(lod.splat.size()));
                for (const auto &splat : lod.splat) {
                    SaveRaw(archive, splat);
                }
            }
        }
    }

    void TerrainAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);

        archive.LoadValue(meta.tileSize);
        archive.LoadValue(meta.resolution);
        archive.LoadValue(meta.heightFormat);
        archive.LoadValue(meta.heightScale);
        archive.LoadValue(meta.heightOffset);
        archive.LoadValue(meta.tileCountX);
        archive.LoadValue(meta.tileCountY);
        archive.LoadValue(meta.layerCount);
        archive.LoadValue(meta.lodCount);
        LoadVec3(archive, meta.origin);

        uint32_t manifestCount = 0;
        archive.LoadValue(manifestCount);
        manifest.resize(manifestCount);
        for (auto &info : manifest) {
            archive.LoadValue(info.coord.x);
            archive.LoadValue(info.coord.y);
            LoadVec3(archive, info.bounds.min);
            LoadVec3(archive, info.bounds.max);
            archive.LoadValue(info.minHeight);
            archive.LoadValue(info.maxHeight);
            archive.LoadValue(info.lodCount);
        }

        uint32_t tileCount = 0;
        archive.LoadValue(tileCount);
        tiles.resize(tileCount);
        for (auto &tile : tiles) {
            archive.LoadValue(tile.coord.x);
            archive.LoadValue(tile.coord.y);
            uint32_t lodCount = 0;
            archive.LoadValue(lodCount);
            tile.lods.resize(lodCount);
            for (auto &lod : tile.lods) {
                LoadRaw(archive, lod.height);
                uint32_t splatCount = 0;
                archive.LoadValue(splatCount);
                lod.splat.resize(splatCount);
                for (auto &splat : lod.splat) {
                    LoadRaw(archive, splat);
                }
            }
        }
    }

    void TerrainAssetData::Reflect(SerializationContext *context)
    {
        context->Register<TerrainAssetData>("TerrainAssetData")
            .BinLoad<&TerrainAssetData::Load>()
            .BinSave<&TerrainAssetData::Save>();
    }

    void RegisterTerrainAssetType()
    {
        TerrainAssetData::Reflect(SerializationContext::Get());
        AssetManager::Get()->RegisterAssetHandler<TerrainAsset>();
    }

} // namespace sky::terrain
