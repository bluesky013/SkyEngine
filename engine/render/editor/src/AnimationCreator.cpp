//
// Created by Zach Lee on 2026/1/11.
//

#include <render/editor/AnimationCreator.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/JsonArchive.h>
#include <render/adaptor/assets/AnimationAsset.h>

namespace sky::editor {

    void AnimationGraphCreator::CreateAsset(const FilePath &path)
    {
        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = path;

        auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);

        AnimationAssetData data = {};
        data.version = 1;

        auto archive = file->WriteAsArchive();
        JsonOutputArchive json(*archive);
        data.SaveJson(json);

        AssetDataBase::Get()->RegisterAsset(sourcePath);
    }

} // namespace sky::editor