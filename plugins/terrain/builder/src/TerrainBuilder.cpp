//
// Created on 2026/09/22.
//

#include <builder/terrain/TerrainBuilder.h>

#include <terrain/TerrainAsset.h>
#include <terrain/TerrainAssetBuilder.h>
#include <terrain/TerrainSource.h>

#include <core/file/FileSystem.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

namespace sky::builder {

    std::string_view TerrainBuilder::QueryType(const std::string &ext) const
    {
        return AssetTraits<terrain::TerrainAsset>::ASSET_TYPE;
    }

    void TerrainBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        terrain::TerrainSourceData source;
        {
            auto archive = request.file->ReadAsArchive();
            if (!archive || !archive->IsOpen()) {
                result.retCode = AssetBuildRetCode::FAILED;
                return;
            }
            JsonInputArchive json(*archive);
            json.LoadValueObject(source);
        }

        terrain::TerrainAssetData data;
        if (!terrain::BuildTerrainAsset(source, data)) {
            result.retCode = AssetBuildRetCode::FAILED;
            return;
        }

        auto asset = AssetManager::Get()->FindOrCreateAsset<terrain::TerrainAsset>(request.assetInfo->uuid);
        asset->Data() = data;
        AssetManager::Get()->SaveAsset(asset, request.target);

        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder
