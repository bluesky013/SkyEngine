//
// Created on 2026/09/21.
//

#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviMeshFactory.h>

#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::ai {

    namespace {
        void SaveVector3(BinaryOutputArchive &archive, const Vector3 &v)
        {
            archive.SaveValue(v.x);
            archive.SaveValue(v.y);
            archive.SaveValue(v.z);
        }

        void LoadVector3(BinaryInputArchive &archive, Vector3 &v)
        {
            archive.LoadValue(v.x);
            archive.LoadValue(v.y);
            archive.LoadValue(v.z);
        }

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
    } // namespace

    void NaviMeshData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(mode);
        archive.SaveValue(params.version);

        archive.SaveValue(params.agent.height);
        archive.SaveValue(params.agent.radius);
        archive.SaveValue(params.agent.maxSlope);
        archive.SaveValue(params.agent.maxClimb);

        archive.SaveValue(params.resolution.cellSize);
        archive.SaveValue(params.resolution.cellHeight);
        archive.SaveValue(params.resolution.tileSize);

        archive.SaveValue(params.maxSimplificationError);
        archive.SaveValue(params.borderSize);

        SaveVector3(archive, params.bounds.min);
        SaveVector3(archive, params.bounds.max);

        archive.SaveValue(static_cast<uint32_t>(tiles.size()));
        for (const auto &tile : tiles) {
            archive.SaveValue(tile.tx);
            archive.SaveValue(tile.ty);
            archive.SaveValue(tile.layer);
            SaveRaw(archive, tile.data);
        }

        SaveRaw(archive, fullData);
    }

    void NaviMeshData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(mode);
        archive.LoadValue(params.version);

        archive.LoadValue(params.agent.height);
        archive.LoadValue(params.agent.radius);
        archive.LoadValue(params.agent.maxSlope);
        archive.LoadValue(params.agent.maxClimb);

        archive.LoadValue(params.resolution.cellSize);
        archive.LoadValue(params.resolution.cellHeight);
        archive.LoadValue(params.resolution.tileSize);

        archive.LoadValue(params.maxSimplificationError);
        archive.LoadValue(params.borderSize);

        LoadVector3(archive, params.bounds.min);
        LoadVector3(archive, params.bounds.max);

        uint32_t tileCount = 0;
        archive.LoadValue(tileCount);
        tiles.resize(tileCount);
        for (auto &tile : tiles) {
            archive.LoadValue(tile.tx);
            archive.LoadValue(tile.ty);
            archive.LoadValue(tile.layer);
            LoadRaw(archive, tile.data);
        }

        LoadRaw(archive, fullData);
    }

    void NaviMeshData::Reflect(SerializationContext *context)
    {
        context->Register<NaviMeshData>("NaviMeshData")
            .BinLoad<&NaviMeshData::Load>()
            .BinSave<&NaviMeshData::Save>();
    }

    CounterPtr<NaviMesh> CreateNaviMeshFromAsset(const NaviMeshAssetPtr &asset)
    {
        if (asset == nullptr) {
            return {};
        }

        return asset->GetOrCreateResource([](Asset<NaviMesh> &inAsset) -> CounterPtr<NaviMesh> {
            auto mesh = NaviMeshFactory::Get()->CreateNaviMesh();
            if (mesh == nullptr) {
                return {};
            }

            if (!mesh->LoadData(inAsset.Data())) {
                return {};
            }

            return mesh;
        });
    }

} // namespace sky::ai
