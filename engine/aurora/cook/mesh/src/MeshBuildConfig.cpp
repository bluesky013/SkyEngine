//
// Mesh cook configuration parsing (see MeshBuildConfig.h).
//

#include <aurora/cook/mesh/MeshBuildConfig.h>

#include <algorithm>
#include <aurora/adaptor/assets/SceneAssets.h>
#include <aurora/cook/mesh/MeshProcess.h>
#include <core/logger/Logger.h>
#include <framework/asset/AssetDataBase.h>
#include <rapidjson/document.h>

static const char *TAG = "AuroraMeshCook";

namespace sky::aurora::cook {

    MeshCookStatus ResolveMeshMaterials(const MeshBuildConfig &config, uint32_t slotCount, std::span<const Uuid> supplied, std::vector<Uuid> &out)
    {
        if (slotCount > 8192 || supplied.size() > 8192)
            return {MeshCookError::InvalidData, "Mesh material slots exceed the product limit"};
        if (!supplied.empty()) {
            if (supplied.size() < slotCount || std::any_of(supplied.begin(), supplied.end(), [](const Uuid &id) { return !id; }))
                return {MeshCookError::InvalidData, "Mesh material slots are incomplete"};
            out.assign(supplied.begin(), supplied.end());
            return {};
        }
        if (!slotCount) {
            out.clear();
            return {};
        }
        Uuid fallback = config.defaultMaterial;
        if (!fallback) {
            const auto source = AssetDataBase::Get()->RegisterAsset("engine", FilePath(scene_assets::UNLIT), false);
            if (!source)
                return {MeshCookError::InvalidData, "Engine default mesh material is unavailable; configure defaultMaterial"};
            fallback = source->uuid;
        }
        out.assign(slotCount, fallback);
        return {};
    }

    MeshCookStatus MeshBuildPresets::LoadJson(std::string_view text)
    {
        rapidjson::Document document;
        document.Parse(text.data(), text.size());
        if (document.HasParseError() || !document.IsObject())
            return {MeshCookError::InvalidData, "Mesh build presets must be a JSON object"};
        MeshBuildPresets candidate;
        candidate.bundles.clear();
        if (const auto entry = document.FindMember("defaultBundle"); entry != document.MemberEnd()) {
            if (!entry->value.IsString())
                return {MeshCookError::InvalidData, "Mesh defaultBundle must be a string"};
            candidate.defaultBundle = entry->value.GetString();
        }
        const auto bundlesEntry = document.FindMember("bundles");
        if (bundlesEntry == document.MemberEnd() || !bundlesEntry->value.IsObject())
            return {MeshCookError::InvalidData, "Mesh build presets require a bundles object"};
        for (const auto &entry : bundlesEntry->value.GetObject()) {
            if (!entry.value.IsObject())
                return {MeshCookError::InvalidData, "Mesh bundle settings must be an object"};
            MeshBuildConfig config;
            if (const auto material = entry.value.FindMember("defaultMaterial"); material != entry.value.MemberEnd()) {
                if (!material->value.IsString())
                    return {MeshCookError::InvalidData, "Mesh defaultMaterial must be a UUID string"};
                const std::string_view idText(material->value.GetString(), material->value.GetStringLength());
                if (!idText.empty()) {
                    if (idText.size() != 36 || idText[8] != '-' || idText[13] != '-' || idText[18] != '-' || idText[23] != '-')
                        return {MeshCookError::InvalidData, "Mesh defaultMaterial must be a valid material UUID"};
                    config.defaultMaterial = Uuid::CreateFromString(idText);
                    if (!config.defaultMaterial)
                        return {MeshCookError::InvalidData, "Mesh defaultMaterial must be a nonzero material UUID"};
                }
            }
            for (const auto &[name, value] :
                 {std::pair{"tangents", &config.tangents}, std::pair{"optimize", &config.optimize}, std::pair{"skinning", &config.skinning}}) {
                if (const auto setting = entry.value.FindMember(name); setting != entry.value.MemberEnd()) {
                    if (!setting->value.IsBool())
                        return {MeshCookError::InvalidData, std::string("Mesh setting must be boolean: ") + name};
                    *value = setting->value.GetBool();
                }
            }
            candidate.bundles.emplace(entry.name.GetString(), std::move(config));
        }
        *this = std::move(candidate);
        return {};
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
