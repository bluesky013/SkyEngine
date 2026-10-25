#include <framework/asset/GeneratedAssetCatalog.h>

#include <algorithm>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <unordered_set>

namespace sky {
    namespace {
        bool ValidText(const std::string &value)
        {
            return !value.empty() && value.size() <= 4096 && value.find('\0') == std::string::npos;
        }

        ProductStatus Validate(const Uuid &owner, const GeneratedAssetManifest &manifest)
        {
            if (!owner || manifest.records.size() > 4096 || manifest.defaults.size() > 256) {
                return {ProductError::InvalidData, "invalid generated asset manifest size or owner"};
            }
            std::unordered_set<Uuid>        ids;
            std::unordered_set<std::string> keys, active;
            for (const auto &record : manifest.records) {
                if (!record.uuid || record.uuid == owner || record.owner != owner || !ValidText(record.key) || record.key == "$source" ||
                    !ValidText(record.type) || !ids.insert(record.uuid).second || !keys.insert(record.key).second) {
                    return {ProductError::InvalidData, "generated asset identities, keys and types must be unique and valid"};
                }
                if (record.active) {
                    active.insert(record.key);
                }
            }
            for (const auto &[role, key] : manifest.defaults) {
                if (!ValidText(role) || (key != "$source" && !active.contains(key))) {
                    return {ProductError::InvalidData, "generated default must reference an active child key"};
                }
            }
            return {};
        }
    } // namespace

    ProductStatus ParseGeneratedAssets(const Uuid &owner, const std::string &json, GeneratedAssetManifest &out)
    {
        GeneratedAssetManifest parsed;
        if (json.empty()) {
            if (!owner) {
                return {ProductError::InvalidData, "generated manifest requires an owner"};
            }
            out = std::move(parsed);
            return {};
        }
        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("version") || !document["version"].IsUint() ||
            document["version"].GetUint() != 1 || !document.HasMember("children") || !document["children"].IsArray() ||
            document["children"].Size() > 4096 || !document.HasMember("defaults") || !document["defaults"].IsObject() ||
            document["defaults"].MemberCount() > 256) {
            return {ProductError::InvalidData, "invalid generated manifest schema or version"};
        }
        for (const auto &value : document["children"].GetArray()) {
            if (!value.IsObject() || !value.HasMember("key") || !value["key"].IsString() || !value.HasMember("type") || !value["type"].IsString() ||
                !value.HasMember("id") || !value["id"].IsString() || !value.HasMember("active") || !value["active"].IsBool()) {
                return {ProductError::InvalidData, "invalid generated child schema"};
            }
            GeneratedAssetRecord record;
            record.uuid  = Uuid::CreateFromString({value["id"].GetString(), value["id"].GetStringLength()});
            record.owner = owner;
            record.key.assign(value["key"].GetString(), value["key"].GetStringLength());
            record.type.assign(value["type"].GetString(), value["type"].GetStringLength());
            record.active = value["active"].GetBool();
            parsed.records.emplace_back(std::move(record));
        }
        for (const auto &value : document["defaults"].GetObject()) {
            if (!value.value.IsString() || !parsed.defaults
                                                .emplace(std::string(value.name.GetString(), value.name.GetStringLength()),
                                                         std::string(value.value.GetString(), value.value.GetStringLength()))
                                                .second) {
                return {ProductError::InvalidData, "invalid or duplicate generated default"};
            }
        }
        if (const auto status = Validate(owner, parsed); !status) {
            return status;
        }
        out = std::move(parsed);
        return {};
    }

    ProductStatus ReconcileGeneratedAssets(const Uuid                               &owner,
                                           const GeneratedAssetManifest             &previous,
                                           std::span<const GeneratedAssetDescriptor> requested,
                                           const std::map<std::string, std::string> &defaults,
                                           GeneratedAssetManifest                   &out)
    {
        if (const auto status = Validate(owner, previous); !status) {
            return status;
        }
        if (requested.size() > 4096) {
            return {ProductError::InvalidData, "too many generated asset requests"};
        }
        auto next     = previous;
        next.defaults = defaults;
        for (auto &record : next.records) {
            record.active = false;
        }
        std::unordered_set<std::string> keys;
        for (const auto &descriptor : requested) {
            if (!ValidText(descriptor.key) || !ValidText(descriptor.type) || !keys.insert(descriptor.key).second) {
                return {ProductError::InvalidData, "invalid or duplicate generated child request"};
            }
            const auto found =
                std::find_if(next.records.begin(), next.records.end(), [&descriptor](const auto &record) { return record.key == descriptor.key; });
            if (found != next.records.end()) {
                if (found->type != descriptor.type || (descriptor.verifiedLegacyId && found->uuid != descriptor.verifiedLegacyId)) {
                    return {ProductError::InvalidData, "generated child key cannot change its asset type or identity"};
                }
                found->active = true;
            } else {
                if (descriptor.verifiedLegacyId && !previous.records.empty()) {
                    return {ProductError::InvalidData, "legacy identity migration requires an empty child catalog"};
                }
                next.records.push_back(
                    {descriptor.verifiedLegacyId ? descriptor.verifiedLegacyId : Uuid::Create(), owner, descriptor.key, descriptor.type, true});
            }
        }
        if (const auto status = Validate(owner, next); !status) {
            return status;
        }
        std::sort(next.records.begin(), next.records.end(), [](const auto &lhs, const auto &rhs) { return lhs.key < rhs.key; });
        out = std::move(next);
        return {};
    }

    std::string SerializeGeneratedAssets(const GeneratedAssetManifest &manifest)
    {
        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        writer.StartObject();
        writer.Key("version");
        writer.Uint(1);
        writer.Key("children");
        writer.StartArray();
        for (const auto &record : manifest.records) {
            writer.StartObject();
            writer.Key("key");
            writer.String(record.key.data(), static_cast<rapidjson::SizeType>(record.key.size()));
            writer.Key("type");
            writer.String(record.type.data(), static_cast<rapidjson::SizeType>(record.type.size()));
            writer.Key("id");
            writer.String(record.uuid.ToString().c_str());
            writer.Key("active");
            writer.Bool(record.active);
            writer.EndObject();
        }
        writer.EndArray();
        writer.Key("defaults");
        writer.StartObject();
        for (const auto &[role, key] : manifest.defaults) {
            writer.Key(role.data(), static_cast<rapidjson::SizeType>(role.size()));
            writer.String(key.data(), static_cast<rapidjson::SizeType>(key.size()));
        }
        writer.EndObject();
        writer.EndObject();
        return {buffer.GetString(), buffer.GetSize()};
    }
} // namespace sky
