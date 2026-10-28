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
        result.retCode = AssetBuildRetCode::FAILED;
        result.error.clear();
        if (!request.assetInfo || !request.file) {
            result.error = "Terrain cook requires an identified source file";
            return;
        }
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

        auto      *manager = AssetManager::Get();
        const auto asset   = std::static_pointer_cast<Asset<terrain::TerrainAsset>>(
            manager->CreateBuildAsset(request.assetInfo->uuid, Name(AssetTraits<terrain::TerrainAsset>::ASSET_TYPE.data())));
        if (!asset) {
            result.error = "Terrain asset handler is unavailable";
            return;
        }
        asset->Data() = std::move(data);
        if (const auto saved = manager->SaveAsset(asset, request.bundle.empty() ? request.target : request.bundle); !saved) {
            result.error = saved.message;
            return;
        }

        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder
