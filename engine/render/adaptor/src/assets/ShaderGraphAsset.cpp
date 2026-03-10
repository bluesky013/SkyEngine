//
// Created by blues on 2026/3/10.
//

#include <render/adaptor/assets/ShaderGraphAsset.h>
#include <framework/serialization/JsonArchive.h>

namespace sky {

    void ShaderGraphAssetData::LoadJson(JsonInputArchive& archive)
    {
        archive.LoadKeyValue("Version", version);
        if (archive.Start("Graph")) {
            graph.LoadJson(archive);
            archive.End();
        }
    }

    void ShaderGraphAssetData::SaveJson(JsonOutputArchive& archive) const
    {
        archive.StartObject();
        archive.SaveValueObject("Version", version);
        archive.Key("Graph");
        graph.SaveJson(archive);
        archive.EndObject();
    }

} // namespace sky
