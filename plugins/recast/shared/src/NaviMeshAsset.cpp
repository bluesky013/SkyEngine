//
// Created on 2026/09/21.
//

#include <navigation/NaviMeshAsset.h>

#include <navigation/NaviMeshFactory.h>

#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>

#include <set>
#include <tuple>
#include <utility>

namespace sky::ai {

    namespace {
        constexpr uint32_t MAX_ASSET_TILES         = 65536;
        constexpr uint64_t MAX_ASSET_PAYLOAD_BYTES = 256ULL * 1024 * 1024;
        constexpr uint64_t MIN_TILE_BYTES          = sizeof(int32_t) * 2 + sizeof(uint32_t) * 2;

        using TileIdentity = std::tuple<int32_t, int32_t, uint32_t>;

        bool RemainingBytes(BinaryInputArchive &archive, uint64_t &remaining)
        {
            auto &stream = archive.GetStream().GetStream();
            if (!archive.IsValid() || !stream)
                return false;
            const auto position = stream.tellg();
            if (position < 0)
                return false;
            stream.seekg(0, std::ios::end);
            const auto end = stream.tellg();
            stream.seekg(position);
            if (!stream || end < position)
                return false;
            remaining = static_cast<uint64_t>(end - position);
            return true;
        }

        bool ValidMode(NaviMeshExportMode mode)
        {
            return mode == NaviMeshExportMode::Tiled || mode == NaviMeshExportMode::Full;
        }

        bool CanSave(const NaviMeshData &data)
        {
            if (!ValidMode(data.mode) || data.params.version != 1 || data.tiles.size() > MAX_ASSET_TILES ||
                data.fullData.size() > MAX_ASSET_PAYLOAD_BYTES)
                return false;
            uint64_t               bytes = data.fullData.size();
            std::set<TileIdentity> seen;
            for (const auto &tile : data.tiles) {
                if (tile.data.size() > MAX_ASSET_PAYLOAD_BYTES - bytes || !seen.emplace(tile.tx, tile.ty, tile.layer).second)
                    return false;
                bytes += tile.data.size();
            }
            return true;
        }

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

        bool LoadRaw(BinaryInputArchive &archive, std::vector<uint8_t> &data, uint64_t reservedBytes, uint64_t &payloadBytes)
        {
            uint32_t size = 0;
            archive.LoadValue(size);
            uint64_t remaining = 0;
            if (size > MAX_ASSET_PAYLOAD_BYTES - payloadBytes || !RemainingBytes(archive, remaining) || reservedBytes > remaining ||
                size > remaining - reservedBytes)
                return false;
            data.resize(size);
            if (size > 0) {
                archive.LoadValue(reinterpret_cast<char *>(data.data()), size);
            }
            payloadBytes += size;
            return archive.IsValid();
        }
    } // namespace

    void NaviMeshData::Save(BinaryOutputArchive &archive) const
    {
        if (!CanSave(*this)) {
            archive.Invalidate();
            return;
        }
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
        NaviMeshData loaded;
        const auto   fail = [&] {
            archive.Invalidate();
            *this = {};
        };
        archive.LoadValue(loaded.mode);
        archive.LoadValue(loaded.params.version);
        if (!archive.IsValid() || !ValidMode(loaded.mode) || loaded.params.version != 1) {
            fail();
            return;
        }

        archive.LoadValue(loaded.params.agent.height);
        archive.LoadValue(loaded.params.agent.radius);
        archive.LoadValue(loaded.params.agent.maxSlope);
        archive.LoadValue(loaded.params.agent.maxClimb);

        archive.LoadValue(loaded.params.resolution.cellSize);
        archive.LoadValue(loaded.params.resolution.cellHeight);
        archive.LoadValue(loaded.params.resolution.tileSize);

        archive.LoadValue(loaded.params.maxSimplificationError);
        archive.LoadValue(loaded.params.borderSize);

        LoadVector3(archive, loaded.params.bounds.min);
        LoadVector3(archive, loaded.params.bounds.max);

        uint32_t tileCount = 0;
        archive.LoadValue(tileCount);
        uint64_t remaining = 0;
        if (tileCount > MAX_ASSET_TILES || !RemainingBytes(archive, remaining) || remaining < sizeof(uint32_t) ||
            tileCount > (remaining - sizeof(uint32_t)) / MIN_TILE_BYTES) {
            fail();
            return;
        }
        loaded.tiles.resize(tileCount);
        uint64_t               payloadBytes = 0;
        std::set<TileIdentity> seen;
        for (uint32_t i = 0; i < tileCount; ++i) {
            auto &tile = loaded.tiles[i];
            archive.LoadValue(tile.tx);
            archive.LoadValue(tile.ty);
            archive.LoadValue(tile.layer);
            const uint64_t reservedBytes = (tileCount - i - 1) * MIN_TILE_BYTES + sizeof(uint32_t);
            if (!seen.emplace(tile.tx, tile.ty, tile.layer).second || !LoadRaw(archive, tile.data, reservedBytes, payloadBytes)) {
                fail();
                return;
            }
        }

        if (!LoadRaw(archive, loaded.fullData, 0, payloadBytes)) {
            fail();
            return;
        }
        *this = std::move(loaded);
    }

    void NaviMeshData::Reflect(SerializationContext *context)
    {
        context->Register<NaviMeshData>("NaviMeshData").BinLoad<&NaviMeshData::Load>().BinSave<&NaviMeshData::Save>();
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
