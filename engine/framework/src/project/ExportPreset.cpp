//
// Created on 2026/10/12.
//

#include <framework/project/ExportPreset.h>

#include <core/archive/FileArchive.h>
#include <core/environment/Once.h>
#include <core/type/TypeInfo.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/SerializationUtil.h>

#include <string_view>

namespace sky {

    namespace {

        // A member is a sequence only when it is a real container: `std::string` also
        // reports a containerInfo, with a `char` value type, so exclude that.
        const ContainerInfo *SequenceInfo(const serialize::TypeMemberNode &member)
        {
            const ContainerInfo *container = member.info->containerInfo;
            if (container != nullptr && container->valueType != TypeInfo<char>::RegisteredId()) {
                return container;
            }
            return nullptr;
        }

        // Flat object form (no reflection `classId`/`elements` envelope), driven by the
        // registered members so the field set lives only in RegisterExportPresetReflection.
        // `cacheVariables` is written as a JSON object to match the packaging CLI.
        void SavePreset(const ExportPreset &value, JsonOutputArchive &ar)
        {
            const Uuid      typeId = TypeInfo<ExportPreset>::RegisteredId();
            const TypeNode *node   = GetTypeNode(typeId);
            ar.StartObject();
            for (const auto &entry : node->members) {
                const std::string name(entry.first);
                const auto       &member = entry.second;
                Any               field  = GetValueRawConst(&value, typeId, name);
                ar.Key(name.c_str());
                if (const ContainerInfo *container = SequenceInfo(member)) {
                    SequenceVisitor visitor(container, field.Data());
                    const size_t    count = visitor.Count();
                    if (container->valueType == TypeInfo<ExportPresetCacheVariable>::RegisteredId()) {
                        ar.StartObject();
                        for (size_t i = 0; i < count; ++i) {
                            const auto *item = static_cast<const ExportPresetCacheVariable *>(visitor.GetByIndex(i));
                            ar.Key(item->key.c_str());
                            ar.SaveValue(item->value);
                        }
                        ar.EndObject();
                    } else {
                        ar.StartArray();
                        for (size_t i = 0; i < count; ++i) {
                            ar.SaveValueObject(visitor.GetByIndex(i), visitor.GetValueType());
                        }
                        ar.EndArray();
                    }
                } else {
                    ar.SaveValueObject(field.Data(), member.info->registeredId);
                }
            }
            ar.EndObject();
        }

        void LoadPreset(ExportPreset &value, JsonInputArchive &ar)
        {
            const Uuid      typeId = TypeInfo<ExportPreset>::RegisteredId();
            const TypeNode *node   = GetTypeNode(typeId);
            for (const auto &entry : node->members) {
                const std::string name(entry.first);
                const auto       &member = entry.second;
                if (!ar.HasMember(name)) {
                    continue;
                }
                Any field = GetValueRawConst(&value, typeId, name);
                if (const ContainerInfo *container = SequenceInfo(member)) {
                    SequenceVisitor visitor(container, field.Data());
                    if (container->valueType == TypeInfo<ExportPresetCacheVariable>::RegisteredId()) {
                        ar.Start(name);
                        ar.ForEachMember([&visitor](const std::string &key, const JsonInputArchive::Value &jsonValue) {
                            auto *item  = static_cast<ExportPresetCacheVariable *>(visitor.Emplace());
                            item->key   = key;
                            item->value = jsonValue.IsString() ? jsonValue.GetString() : std::string();
                        });
                        ar.End();
                    } else {
                        const uint32_t count = ar.StartArray(name);
                        for (uint32_t i = 0; i < count; ++i) {
                            auto *element = visitor.Emplace();
                            ar.LoadValueById(element, visitor.GetValueType());
                            ar.NextArrayElement();
                        }
                        ar.End();
                    }
                } else {
                    ar.Start(name);
                    ar.LoadValueById(field.Data(), member.info->registeredId);
                    ar.End();
                }
                SetValueRaw(&value, typeId, name, field.Data());
            }
        }

        // The document is a bare array of presets, or an object with a `presets` array.
        bool LoadPresets(ExportPresets &value, JsonInputArchive &ar)
        {
            if (!ar.IsRootArray() && !ar.HasMember("presets")) {
                return false;
            }
            const uint32_t count = ar.IsRootArray() ? ar.StartRootArray() : ar.StartArray("presets");
            for (uint32_t i = 0; i < count; ++i) {
                ExportPreset preset;
                ar.LoadValueById(&preset, TypeInfo<ExportPreset>::RegisteredId());
                ar.NextArrayElement();
                value.presets.emplace_back(std::move(preset));
            }
            ar.End();
            return true;
        }

        void SavePresets(const ExportPresets &value, JsonOutputArchive &ar)
        {
            ar.StartArray();
            for (const auto &preset : value.presets) {
                ar.SaveValueObject(&preset, TypeInfo<ExportPreset>::RegisteredId());
            }
            ar.EndArray();
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
        RegisterExportPresetReflection();
        IFileArchive in(FilePath{path});
        if (!in.IsOpen()) {
            return false;
        }
        JsonInputArchive json(in);
        return LoadPresets(*this, json);
    }

    bool ExportPresets::Save(const std::string &path) const
    {
        RegisterExportPresetReflection();
        OFileArchive out(FilePath{path}, std::ios::binary | std::ios::trunc);
        if (!out.IsOpen()) {
            return false;
        }
        JsonOutputArchive json(out);
        SavePresets(*this, json);
        return true;
    }

    void RegisterExportPresetReflection()
    {
        if (!Once::Get()->Run("ExportPresetReflection")) {
            return;
        }

        SerializationContext::Get()
            ->Register<ExportPresetCacheVariable>("ExportPresetCacheVariable")
            .Member<&ExportPresetCacheVariable::key>("key")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Key")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(1))
            .Member<&ExportPresetCacheVariable::value>("value")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Value")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(2));

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
            .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Build")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(46))
            .Member<&ExportPreset::shaderTargets>("shaderTargets")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Shader Targets (vulkan|metal|d3d12)")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Build")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(47))
            .Member<&ExportPreset::cacheVariables>("cacheVariables")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Cache Variables")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Build")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(50))
            .Member<&ExportPreset::compileDefinitions>("compileDefinitions")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Compile Definitions")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Build")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(51))
            .Member<&ExportPreset::compileFlags>("compileFlags")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Compile Flags")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Build")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(52))
            .Member<&ExportPreset::extraConfigureArgs>("extraConfigureArgs")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Extra Configure Args")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Build")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(53))
            .Member<&ExportPreset::extraBuildArgs>("extraBuildArgs")
            .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Extra Build Args")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::CATEGORY), Any(std::string_view("Build")))
            .Property(static_cast<uint32_t>(CommonPropertyKey::ORDER), Any(54))
            .JsonSave<&SavePreset>()
            .JsonLoad<&LoadPreset>();

        // The whole document (a sequence of presets) so a single reflected form can
        // edit every preset without a hand-written list widget.
        SerializationContext::Get()
            ->Register<ExportPresets>("ExportPresets")
            .Member<&ExportPresets::presets>("presets")
            .JsonSave<&SavePresets>()
            .JsonLoad<&LoadPresets>();
    }

} // namespace sky
