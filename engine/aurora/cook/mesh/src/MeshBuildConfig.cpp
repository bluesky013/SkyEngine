//
// Mesh cook configuration parsing (see MeshBuildConfig.h).
//

#include <aurora/cook/mesh/MeshBuildConfig.h>

#include <core/logger/Logger.h>
#include <framework/serialization/JsonArchive.h>

static const char *TAG = "AuroraMeshCook";

namespace sky::aurora::cook {

    void MeshBuildPresets::LoadJson(JsonInputArchive &json)
    {
        if (json.Start("defaultBundle")) {
            defaultBundle = json.LoadString();
            json.End();
        }

        if (!json.Start("bundles")) {
            return;
        }

        json.ForEachMember([&json, this](const std::string &key) {
            MeshBuildConfig cfg;
            json.Start(key);

            if (json.Start("tangents")) {
                cfg.tangents = json.LoadBool();
                json.End();
            }
            if (json.Start("optimize")) {
                cfg.optimize = json.LoadBool();
                json.End();
            }
            if (json.Start("meshlets")) {
                cfg.meshlets = json.LoadBool();
                json.End();
            }
            if (json.Start("skinning")) {
                cfg.skinning = json.LoadBool();
                json.End();
            }

            json.End();
            bundles[key] = cfg;
        });

        json.End();
    }

    const MeshBuildConfig *MeshBuildPresets::Resolve(const std::string &requested, std::string &resolvedKey) const
    {
        if (!requested.empty()) {
            auto iter = bundles.find(requested);
            if (iter != bundles.end()) {
                resolvedKey = iter->first;
                return &iter->second;
            }
            LOG_W(TAG, "unknown mesh bundle '%s', falling back to '%s'", requested.c_str(), defaultBundle.c_str());
        }

        auto iter = bundles.find(defaultBundle);
        if (iter != bundles.end()) {
            resolvedKey = iter->first;
            return &iter->second;
        }
        return nullptr;
    }

} // namespace sky::aurora::cook
