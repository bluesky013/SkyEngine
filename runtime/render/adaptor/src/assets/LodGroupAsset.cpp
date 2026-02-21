//
// Created by blues on 2026/2/17.
//

#include <render/adaptor/assets/LodGroupAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/lod/LodGroup.h>
#include <render/mesh/MeshLodProxy.h>
#include <core/profile/Profiler.h>

namespace sky {

    void LodGroupData::LoadBin(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);

        archive.LoadValue(type);

        uint32_t num = 0;
        archive.LoadValue(num);

        levels.resize(num);
        for (uint32_t i = 0; i < num; ++i) {
            archive.LoadValue(levels[i].screenSize);
            archive.LoadValue(levels[i].resId);
        }
    }

    void LodGroupData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(type);

        archive.SaveValue(static_cast<uint32_t>(levels.size()));
        for (const auto& level : levels) {
            archive.SaveValue(level.screenSize);
            archive.SaveValue(level.resId);
        }
    }

    void LodGroupData::LoadJson(JsonInputArchive &archive)
    {
        archive.Start("Type");
        type = archive.LoadString();
        archive.End();

        uint32_t num = archive.StartArray("Levels");
        levels.resize(num);
        for (uint32_t i = 0; i < num; ++i) {
            archive.Start("ScreenSize");
            levels[i].screenSize = static_cast<float>(archive.LoadDouble());
            archive.End();

            archive.Start("ResId");
            levels[i].resId = Uuid::CreateFromString(archive.LoadString());
            archive.End();
            archive.NextArrayElement();
        }
    }

    void LodGroupData::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();

        archive.Key("Type");
        archive.SaveValue(type);

        archive.Key("Levels");
        archive.StartArray();
        for (const auto& level : levels) {
            archive.StartObject();

            archive.Key("ScreenSize");
            archive.SaveValue(level.screenSize);

            archive.Key("ResId");
            archive.SaveValue(level.resId.ToString());

            archive.EndObject();
        }
        archive.EndArray();
    }

    CounterPtr<LodGroup> CreateLodGroupFromAsset(const LodGroupAssetPtr &asset)
    {
        SKY_PROFILE_NAME("Create Mesh From Asset")

        const auto& lodData = asset->Data();

        auto *group = new LodGroup();
        group->Init(lodData.levels.size());

        for (uint32_t i = 0; i < lodData.levels.size(); ++i) {
            const auto& levelData = lodData.levels[i];
            auto meshAsset = AssetManager::Get()->LoadAsset<Mesh>(levelData.resId);
            if (meshAsset == nullptr) {
                continue;
            }
            auto mesh = CreateMeshFromAsset(meshAsset);
            LodLevel lodLevel { levelData.screenSize };

            auto *proxy = new MeshLodProxy(mesh, lodLevel);
            group->AddLod(proxy, i);
        }

        return group;
    }

} // namespace sky