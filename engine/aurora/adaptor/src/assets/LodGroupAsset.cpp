//
// LodGroupAssetData Bin save/load (versioned ordered levels of screenSize + mesh Uuid).
//

#include <aurora/adaptor/assets/LodGroupAsset.h>

#include <core/logger/Logger.h>

#include <string>

static const char *TAG = "AuroraLodGroupAsset";

namespace sky::aurora {

    void LodGroupAssetData::Save(BinaryOutputArchive &ar) const
    {
        ar.SaveValue(version);
        ar.SaveValue(static_cast<uint32_t>(levels.size()));
        for (const auto &level : levels) {
            ar.SaveValue(level.screenSize);
            ar.SaveValue(level.mesh.ToString());
        }
    }

    void LodGroupAssetData::Load(BinaryInputArchive &ar)
    {
        ar.LoadValue(version);
        if (version != CURRENT_VERSION) {
            LOG_E(TAG, "unsupported lod group asset version: %u (expected %u)", version, CURRENT_VERSION);
            clear();
            return;
        }

        uint32_t count = 0;
        ar.LoadValue(count);
        levels.resize(count);
        for (auto &level : levels) {
            ar.LoadValue(level.screenSize);
            std::string meshStr;
            ar.LoadValue(meshStr);
            level.mesh = Uuid::CreateFromString(meshStr);
        }
    }

    void LodGroupAssetData::clear()
    {
        levels.clear();
    }

} // namespace sky::aurora
