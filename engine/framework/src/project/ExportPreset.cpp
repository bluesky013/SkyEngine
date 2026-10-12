//
// Created on 2026/10/12.
//

#include <framework/project/ExportPreset.h>

#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

#include <rapidjson/document.h>

#include <fstream>
#include <sstream>
#include <string_view>

namespace sky {

    namespace {
        std::string Quote(const std::string &value)
        {
            return "\"" + value + "\"";
        }

        std::string QuoteArray(const std::vector<std::string> &values)
        {
            std::string out = "[";
            for (size_t i = 0; i < values.size(); ++i) {
                out += (i == 0 ? "" : ", ") + Quote(values[i]);
            }
            out += "]";
            return out;
        }

        std::vector<std::string> ReadArray(const rapidjson::Value &value)
        {
            std::vector<std::string> out;
            if (value.IsArray()) {
                for (const auto &entry : value.GetArray()) {
                    if (entry.IsString()) {
                        out.emplace_back(entry.GetString());
                    }
                }
            }
            return out;
        }

        std::string ReadString(const rapidjson::Value &object, const char *key, const std::string &fallback = {})
        {
            if (object.HasMember(key) && object[key].IsString()) {
                return object[key].GetString();
            }
            return fallback;
        }

        int ReadInt(const rapidjson::Value &object, const char *key, int fallback)
        {
            return (object.HasMember(key) && object[key].IsInt()) ? object[key].GetInt() : fallback;
        }

        bool ReadBool(const rapidjson::Value &object, const char *key, bool fallback)
        {
            return (object.HasMember(key) && object[key].IsBool()) ? object[key].GetBool() : fallback;
        }
    } // namespace

    const ExportPreset *ExportPresets::Find(const std::string &name) const
    {
        for (const auto &preset : presets) {
            if (preset.name == name) {
                return &preset;
            }
        }
        return nullptr;
    }

    bool ExportPresets::Load(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream ss;
        ss << in.rdbuf();

        rapidjson::Document document;
        document.Parse(ss.str().c_str());
        if (document.HasParseError()) {
            return false;
        }

        const rapidjson::Value *array = nullptr;
        if (document.IsArray()) {
            array = &document;
        } else if (document.IsObject() && document.HasMember("presets") && document["presets"].IsArray()) {
            array = &document["presets"];
        } else {
            return false;
        }

        presets.clear();
        for (const auto &entry : array->GetArray()) {
            if (!entry.IsObject()) {
                continue;
            }
            ExportPreset preset;
            preset.name                = ReadString(entry, "name");
            preset.platform            = ReadString(entry, "platform");
            preset.variant             = ReadString(entry, "variant", "debug");
            preset.bundleId            = ReadString(entry, "bundleId");
            preset.versionName         = ReadString(entry, "versionName", "1.0");
            preset.versionCode         = ReadInt(entry, "versionCode", 1);
            preset.signing             = ReadString(entry, "signing", "auto");
            preset.bundle              = ReadBool(entry, "bundle", false);
            preset.team                = ReadString(entry, "team");
            preset.signingIdentity     = ReadString(entry, "signingIdentity");
            preset.provisioningProfile = ReadString(entry, "provisioningProfile");
            preset.keystore            = ReadString(entry, "keystore");
            preset.compileSdk          = ReadInt(entry, "compileSdk", 34);
            preset.minSdk              = ReadInt(entry, "minSdk", 29);
            preset.targetSdk           = ReadInt(entry, "targetSdk", 33);
            preset.ndkVersion          = ReadString(entry, "ndkVersion", "28.1.13356709");
            preset.packing             = ReadString(entry, "packing", "dynamic");
            if (entry.HasMember("bundles")) {
                preset.bundles = ReadArray(entry["bundles"]);
            }
            if (entry.HasMember("abis")) {
                preset.abis = ReadArray(entry["abis"]);
            }
            presets.emplace_back(std::move(preset));
        }
        return true;
    }

    bool ExportPresets::Save(const std::string &path) const
    {
        std::string json = "[\n";
        for (size_t i = 0; i < presets.size(); ++i) {
            const ExportPreset &p = presets[i];
            json += "  {";
            json += " \"name\": " + Quote(p.name);
            json += ", \"platform\": " + Quote(p.platform);
            json += ", \"variant\": " + Quote(p.variant);
            json += ", \"bundleId\": " + Quote(p.bundleId);
            json += ", \"versionName\": " + Quote(p.versionName);
            json += ", \"versionCode\": " + std::to_string(p.versionCode);
            json += ", \"signing\": " + Quote(p.signing);
            json += std::string(", \"bundle\": ") + (p.bundle ? "true" : "false");
            json += ", \"team\": " + Quote(p.team);
            json += ", \"signingIdentity\": " + Quote(p.signingIdentity);
            json += ", \"provisioningProfile\": " + Quote(p.provisioningProfile);
            json += ", \"bundles\": " + QuoteArray(p.bundles);
            json += ", \"abis\": " + QuoteArray(p.abis);
            json += ", \"keystore\": " + Quote(p.keystore);
            json += ", \"compileSdk\": " + std::to_string(p.compileSdk);
            json += ", \"minSdk\": " + std::to_string(p.minSdk);
            json += ", \"targetSdk\": " + std::to_string(p.targetSdk);
            json += ", \"ndkVersion\": " + Quote(p.ndkVersion);
            json += ", \"packing\": " + Quote(p.packing);
            json += " }";
            json += (i + 1 == presets.size()) ? "\n" : ",\n";
        }
        json += "]\n";

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << json;
        return static_cast<bool>(out);
    }

    void RegisterExportPresetReflection()
    {
        static bool done = false;
        if (done) {
            return;
        }
        done = true;

        SerializationContext::Get()
            ->Register<ExportPreset>("ExportPreset")
            .Member<&ExportPreset::name>("name")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Name")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(1))
            .Member<&ExportPreset::platform>("platform")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Platform")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Identity")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(2))
            .Member<&ExportPreset::variant>("variant")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Variant")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Identity")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(3))
            .Member<&ExportPreset::bundleId>("bundleId")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Bundle Id")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Identity")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(4))
            .Member<&ExportPreset::versionName>("versionName")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Version Name")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Identity")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(5))
            .Member<&ExportPreset::versionCode>("versionCode")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Version Code")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Identity")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(6))
                .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.0f))
            .Member<&ExportPreset::bundles>("bundles")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Bundles")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Content")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(20))
            .Member<&ExportPreset::signing>("signing")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Signing (auto|off)")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Apple")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(30))
            .Member<&ExportPreset::bundle>("bundle")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("macOS .app Bundle")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Apple")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(31))
            .Member<&ExportPreset::team>("team")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Team Id")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Apple")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(32))
            .Member<&ExportPreset::signingIdentity>("signingIdentity")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Signing Identity")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Apple")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(33))
            .Member<&ExportPreset::provisioningProfile>("provisioningProfile")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Provisioning Profile")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Apple")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(34))
            .Member<&ExportPreset::abis>("abis")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("ABIs")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Android")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(40))
            .Member<&ExportPreset::keystore>("keystore")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Keystore")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Android")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(41))
            .Member<&ExportPreset::compileSdk>("compileSdk")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Compile SDK")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Android")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(42))
            .Member<&ExportPreset::minSdk>("minSdk")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Min SDK")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Android")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(43))
            .Member<&ExportPreset::targetSdk>("targetSdk")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Target SDK")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Android")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(44))
            .Member<&ExportPreset::ndkVersion>("ndkVersion")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("NDK Version")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Android")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(45))
            .Member<&ExportPreset::packing>("packing")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Module Packing (dynamic|static)")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(50))
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("NDK Version")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Android")))
                .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(45));

        // The whole document (a sequence of presets) so a single reflected form can
        // edit every preset without a hand-written list widget.
        SerializationContext::Get()->Register<ExportPresets>("ExportPresets").Member<&ExportPresets::presets>("presets");
    }

} // namespace sky
