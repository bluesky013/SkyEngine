//
// Created by blues on 2026/3/10.
//

#include <render/editor/ShaderGraphCreator.h>
#include <render/adaptor/assets/ShaderGraphAsset.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/JsonArchive.h>

namespace sky::editor {

    void ShaderGraphCreator::CreateAsset(const FilePath& path)
    {
        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = path;

        auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);

        ShaderGraphAssetData data = {};
        data.version = 1;

        auto archive = file->WriteAsArchive();
        JsonOutputArchive json(*archive);
        data.SaveJson(json);

        AssetDataBase::Get()->RegisterAsset(sourcePath);
    }

} // namespace sky::editor
