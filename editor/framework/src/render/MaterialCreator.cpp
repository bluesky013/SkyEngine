//
// Created by blues on 2024/12/7.
//

#include <editor/framework/render/MaterialCreator.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/JsonArchive.h>

namespace sky::editor {

    void MaterialInstanceCreator::CreateAsset(const sky::FilePath &path)
    {
        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = path;

        auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
        AssetDataBase::Get()->RegisterAsset(sourcePath);

        MaterialInstanceData data = {};
        data.material = AssetDataBase::Get()->RegisterAsset("materials/standard_pbr.mat")->uuid;

        auto archive = file->WriteAsArchive();
        JsonOutputArchive json(*archive);
        data.SaveJson(json);
    }

} // namespace sky::editor