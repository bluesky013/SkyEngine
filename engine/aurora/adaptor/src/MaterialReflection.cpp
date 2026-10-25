//
// Material reflection: hand-written JSON codecs for the material source (`MaterialAssetData` /
// `MaterialInstanceSource`) so `.mat` / `.mati` are (de)serialized in a concise authored schema,
// bypassing the reflection classId/elements wrapper. Registered on the types; cook + editor use
// LoadValueObject / SaveValueObject.
//

#include <aurora/adaptor/assets/MaterialReflection.h>

#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

namespace sky::aurora {

    namespace {

        void LoadPropertyValueJson(MaterialPropertyValue &out, uint32_t index, const rapidjson::Value &v)
        {
            Sampler::Descriptor invalid;
            invalid.minFilter  = static_cast<Filter>(UINT32_MAX);
            out                = invalid;
            const auto numbers = [&](rapidjson::SizeType count) {
                return v.IsArray() && v.Size() == count && std::all_of(v.Begin(), v.End(), [](const auto &item) { return item.IsNumber(); });
            };
            if (index == 0 && v.IsNumber()) {
                out = v.IsNumber() ? static_cast<float>(v.GetDouble()) : 0.0f;
            } else if (index == 1 && numbers(2)) {
                Vector2 x{};
                for (int i = 0; i < 2; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 2 && numbers(3)) {
                Vector3 x{};
                for (int i = 0; i < 3; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 3 && numbers(4)) {
                Vector4 x{};
                for (int i = 0; i < 4; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 4 && numbers(16)) {
                Matrix4 x{};
                for (int i = 0; i < 16; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 5 && v.IsInt()) {
                out = v.IsInt() ? v.GetInt() : 0;
            } else if (index == 6 && v.IsUint()) {
                out = v.IsUint() ? v.GetUint() : 0u;
            } else if (index == 7 && v.IsBool()) {
                out = v.IsBool() ? v.GetBool() : false;
            } else if (index == 8 && v.IsString()) {
                out = v.IsString() ? Uuid::CreateFromString(v.GetString()) : Uuid{};
            } else if (index == 9) {
                Sampler::Descriptor sampler;
                bool                valid = v.IsObject();
                if (valid) {
                    const auto integer = [&](const char *key, auto &field) {
                        if (!v.HasMember(key))
                            return;
                        if (!v[key].IsUint()) {
                            valid = false;
                            return;
                        }
                        field = static_cast<std::decay_t<decltype(field)>>(v[key].GetUint());
                    };
                    integer("magFilter", sampler.magFilter);
                    integer("minFilter", sampler.minFilter);
                    integer("mipmapMode", sampler.mipmapMode);
                    integer("addressModeU", sampler.addressModeU);
                    integer("addressModeV", sampler.addressModeV);
                    integer("addressModeW", sampler.addressModeW);
                    for (const auto &[name, field] :
                         {std::pair{"minLod", &sampler.minLod}, {"maxLod", &sampler.maxLod}, {"maxAnisotropy", &sampler.maxAnisotropy}})
                        if (v.HasMember(name)) {
                            if (v[name].IsNumber())
                                *field = static_cast<float>(v[name].GetDouble());
                            else
                                valid = false;
                        }
                    if (v.HasMember("anisotropyEnable")) {
                        if (v["anisotropyEnable"].IsBool())
                            sampler.anisotropyEnable = v["anisotropyEnable"].GetBool();
                        else
                            valid = false;
                    }
                }
                if (!valid)
                    sampler.minFilter = static_cast<Filter>(UINT32_MAX);
                out = sampler;
            }
        }

        void LoadPropertyEntryJson(const rapidjson::Value &v, MaterialPropertyData &out)
        {
            if (v.HasMember("name") && v["name"].IsString()) {
                out.name = v["name"].GetString();
            }
            if (v.HasMember("type") && v["type"].IsUint()) {
                out.type = static_cast<MaterialPropertyType>(v["type"].GetUint());
            }
            if (v.HasMember("value") && v["value"].IsObject()) {
                const rapidjson::Value &value = v["value"];
                const uint32_t          index = value.HasMember("index") && value["index"].IsUint() ? value["index"].GetUint() : 0u;
                if (value.HasMember("value")) {
                    LoadPropertyValueJson(out.value, index, value["value"]);
                }
            }
        }

        void LoadRenderStateJson(const rapidjson::Value &v, MaterialRenderState &out)
        {
            if (v.HasMember("depthStencil") && v["depthStencil"].IsObject()) {
                const rapidjson::Value &ds = v["depthStencil"];
                if (ds.HasMember("depthTest") && ds["depthTest"].IsBool()) {
                    out.depthStencil.depthTest = ds["depthTest"].GetBool();
                }
                if (ds.HasMember("depthWrite") && ds["depthWrite"].IsBool()) {
                    out.depthStencil.depthWrite = ds["depthWrite"].GetBool();
                }
                if (ds.HasMember("stencilTest") && ds["stencilTest"].IsBool()) {
                    out.depthStencil.stencilTest = ds["stencilTest"].GetBool();
                }
                if (ds.HasMember("compareOp") && ds["compareOp"].IsUint()) {
                    out.depthStencil.compareOp = static_cast<CompareOp>(ds["compareOp"].GetUint());
                }
            }
            if (v.HasMember("rasterState") && v["rasterState"].IsObject()) {
                const rapidjson::Value &rs = v["rasterState"];
                if (rs.HasMember("cullMode") && rs["cullMode"].IsUint()) {
                    out.rasterState.cullMode = CullingModeFlags(static_cast<CullModeFlagBits>(rs["cullMode"].GetUint()));
                }
                if (rs.HasMember("frontFace") && rs["frontFace"].IsUint()) {
                    out.rasterState.frontFace = static_cast<FrontFace>(rs["frontFace"].GetUint());
                }
                if (rs.HasMember("polygonMode") && rs["polygonMode"].IsUint()) {
                    out.rasterState.polygonMode = static_cast<PolygonMode>(rs["polygonMode"].GetUint());
                }
                if (rs.HasMember("depthBiasConstantFactor") && rs["depthBiasConstantFactor"].IsNumber()) {
                    out.rasterState.depthBiasConstantFactor = static_cast<float>(rs["depthBiasConstantFactor"].GetDouble());
                    out.rasterState.depthBiasEnable         = true;
                }
                if (rs.HasMember("depthBiasSlopeFactor") && rs["depthBiasSlopeFactor"].IsNumber()) {
                    out.rasterState.depthBiasSlopeFactor = static_cast<float>(rs["depthBiasSlopeFactor"].GetDouble());
                    out.rasterState.depthBiasEnable      = true;
                }
                if (rs.HasMember("depthBiasClamp") && rs["depthBiasClamp"].IsNumber()) {
                    out.rasterState.depthBiasClamp  = static_cast<float>(rs["depthBiasClamp"].GetDouble());
                    out.rasterState.depthBiasEnable = true;
                }
            }
            if (v.HasMember("alphaToCoverage") && v["alphaToCoverage"].IsBool()) {
                out.alphaToCoverage = v["alphaToCoverage"].GetBool();
            }
        }

        bool LoadProgramJson(const rapidjson::Value &program, ShaderTechnique &out)
        {
            for (const auto &[name, ref] : {std::pair{"vertex", &out.vertex}, {"fragment", &out.fragment}, {"compute", &out.compute}}) {
                if (!program.HasMember(name)) {
                    continue;
                }
                const auto &stage = program[name];
                if (!stage.IsObject() || !stage.HasMember("relativePath") || !stage["relativePath"].IsString() || !stage.HasMember("entry") ||
                    !stage["entry"].IsString()) {
                    return false;
                }
                ref->relativePath = stage["relativePath"].GetString();
                ref->entry        = stage["entry"].GetString();
            }
            if (!program.HasMember("variant")) {
                return true;
            }
            if (!program["variant"].IsObject()) {
                return false;
            }
            for (auto entry = program["variant"].MemberBegin(); entry != program["variant"].MemberEnd(); ++entry) {
                ShaderVariantEntry variant{.key = Name(entry->name.GetString())};
                const auto        &value = entry->value;
                if (value.IsUint()) {
                    variant.value = value.GetUint();
                } else if (value.IsBool()) {
                    variant.type  = ShaderVariantType::BOOL;
                    variant.value = value.GetBool() ? 1u : 0u;
                } else if (value.IsInt()) {
                    variant.type  = ShaderVariantType::INT32;
                    variant.value = static_cast<uint32_t>(value.GetInt());
                } else if (value.IsObject() && value.MemberCount() == 2 && value.HasMember("type") && value["type"].IsUint() &&
                           value["type"].GetUint() <= static_cast<uint32_t>(ShaderVariantType::BOOL) && value.HasMember("value") &&
                           value["value"].IsUint()) {
                    variant.type  = static_cast<ShaderVariantType>(value["type"].GetUint());
                    variant.value = value["value"].GetUint();
                } else {
                    return false;
                }
                if (variant.type == ShaderVariantType::BOOL && variant.value > 1)
                    return false;
                out.variant.entries.push_back(variant);
            }
            return true;
        }
        bool LoadAliasesJson(const rapidjson::Value &values, std::vector<ShaderParameterAlias> &out)
        {
            if (!values.IsArray() || values.Size() > 1024)
                return false;
            for (const auto &value : values.GetArray()) {
                if (!value.IsObject() || !value.HasMember("alias") || !value["alias"].IsString() || !value.HasMember("target") ||
                    !value["target"].IsString())
                    return false;
                out.push_back({value["alias"].GetString(), value["target"].GetString()});
            }
            return true;
        }
        bool LoadTechniqueJson(const rapidjson::Value &v, MaterialTechniqueData &out)
        {
            if (!v.HasMember("tag") || !v["tag"].IsString() || !v.HasMember("program") || !v["program"].IsObject()) {
                return false;
            }
            out.tag = v["tag"].GetString();
            if (!LoadProgramJson(v["program"], out.program)) {
                return false;
            }
            if (v.HasMember("renderState") && v["renderState"].IsObject()) {
                LoadRenderStateJson(v["renderState"], out.renderState);
            }
            return !v.HasMember("uniforms") && (!v.HasMember("aliases") || LoadAliasesJson(v["aliases"], out.aliases));
        }

        std::string CompareOpText(CompareOp op)
        {
            static const std::pair<CompareOp, const char *> kTable[] = {{CompareOp::NEVER, "never"},
                                                                        {CompareOp::LESS, "less"},
                                                                        {CompareOp::EQUAL, "equal"},
                                                                        {CompareOp::LESS_OR_EQUAL, "less_equal"},
                                                                        {CompareOp::GREATER, "greater"},
                                                                        {CompareOp::NOT_EQUAL, "not_equal"},
                                                                        {CompareOp::GREATER_OR_EQUAL, "greater_equal"},
                                                                        {CompareOp::ALWAYS, "always"}};
            for (const auto &e : kTable) {
                if (e.first == op) {
                    return e.second;
                }
            }
            return "less_equal";
        }

        void SavePropertyValueJson(sky::JsonOutputArchive &ar, const MaterialPropertyValue &value)
        {
            ar.StartObject();
            ar.Key("index");
            ar.SaveValue(static_cast<uint32_t>(value.index()));
            ar.Key("value");
            std::visit(
                [&ar](const auto &v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, float>) {
                        ar.SaveValue(static_cast<double>(v));
                    } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, bool>) {
                        ar.SaveValue(v);
                    } else if constexpr (std::is_same_v<T, Uuid>) {
                        ar.SaveValue(v.ToString());
                    } else if constexpr (std::is_same_v<T, Sampler::Descriptor>) {
                        ar.StartObject();
                        ar.Key("magFilter");
                        ar.SaveValue(static_cast<uint32_t>(v.magFilter));
                        ar.Key("minFilter");
                        ar.SaveValue(static_cast<uint32_t>(v.minFilter));
                        ar.Key("mipmapMode");
                        ar.SaveValue(static_cast<uint32_t>(v.mipmapMode));
                        ar.Key("addressModeU");
                        ar.SaveValue(static_cast<uint32_t>(v.addressModeU));
                        ar.Key("addressModeV");
                        ar.SaveValue(static_cast<uint32_t>(v.addressModeV));
                        ar.Key("addressModeW");
                        ar.SaveValue(static_cast<uint32_t>(v.addressModeW));
                        ar.Key("minLod");
                        ar.SaveValue(v.minLod);
                        ar.Key("maxLod");
                        ar.SaveValue(v.maxLod);
                        ar.Key("maxAnisotropy");
                        ar.SaveValue(v.maxAnisotropy);
                        ar.Key("anisotropyEnable");
                        ar.SaveValue(v.anisotropyEnable);
                        ar.EndObject();
                    } else {
                        constexpr std::size_t n = sizeof(v.v) / sizeof(float);
                        ar.StartArray();
                        for (std::size_t i = 0; i < n; ++i) {
                            ar.SaveValue(static_cast<double>(v.v[i]));
                        }
                        ar.EndArray();
                    }
                },
                value);
            ar.EndObject();
        }

        void SavePropertyEntryJson(sky::JsonOutputArchive &ar, const MaterialPropertyData &p)
        {
            ar.StartObject();
            ar.Key("name");
            ar.SaveValue(p.name);
            ar.Key("type");
            ar.SaveValue(static_cast<uint32_t>(p.type));
            ar.Key("value");
            SavePropertyValueJson(ar, p.value);
            ar.EndObject();
        }

        void SaveRenderStateJson(sky::JsonOutputArchive &ar, const MaterialRenderState &rs)
        {
            ar.StartObject();
            ar.Key("depthStencil");
            ar.StartObject();
            ar.Key("depthTest");
            ar.SaveValue(rs.depthStencil.depthTest);
            ar.Key("depthWrite");
            ar.SaveValue(rs.depthStencil.depthWrite);
            if (rs.depthStencil.stencilTest) {
                ar.Key("stencilTest");
                ar.SaveValue(true);
            }
            ar.EndObject();
            ar.Key("rasterState");
            ar.StartObject();
            if (rs.rasterState.cullMode != CullingModeFlags(CullModeFlagBits::NONE)) {
                ar.Key("cullMode");
                ar.SaveValue(static_cast<uint32_t>(rs.rasterState.cullMode.value));
            }
            ar.EndObject();
            if (rs.alphaToCoverage) {
                ar.Key("alphaToCoverage");
                ar.SaveValue(true);
            }
            ar.EndObject();
        }

        void SaveTechniqueJson(sky::JsonOutputArchive &ar, const MaterialTechniqueData &t)
        {
            ar.StartObject();
            ar.Key("tag");
            ar.SaveValue(t.tag);
            ar.Key("program");
            ar.StartObject();
            for (const auto &[name, ref] :
                 {std::pair{"vertex", &t.program.vertex}, {"fragment", &t.program.fragment}, {"compute", &t.program.compute}}) {
                if (ref->relativePath.empty() && ref->entry.empty()) {
                    continue;
                }
                ar.Key(name);
                ar.StartObject();
                ar.Key("relativePath");
                ar.SaveValue(ref->relativePath);
                ar.Key("entry");
                ar.SaveValue(ref->entry);
                ar.EndObject();
            }
            ar.Key("variant");
            ar.StartObject();
            for (const auto &entry : t.program.variant.entries) {
                ar.Key(std::string(entry.key.GetStr()).c_str());
                ar.StartObject();
                ar.Key("type");
                ar.SaveValue(static_cast<uint32_t>(entry.type));
                ar.Key("value");
                ar.SaveValue(entry.value);
                ar.EndObject();
            }
            ar.EndObject();
            ar.EndObject();
            ar.Key("renderState");
            SaveRenderStateJson(ar, t.renderState);
            ar.Key("aliases");
            ar.StartArray();
            for (const auto &alias : t.aliases) {
                ar.StartObject();
                ar.Key("alias");
                ar.SaveValue(alias.alias);
                ar.Key("target");
                ar.SaveValue(alias.target);
                ar.EndObject();
            }
            ar.EndArray();
            ar.EndObject();
        }

    } // namespace

    void JsonLoadMaterialAssetData(MaterialAssetData &value, sky::JsonInputArchive &archive)
    {
        value.clear();
        value.version = MaterialAssetData::CURRENT_VERSION;
        archive.ForEachMember([&value](const std::string &name, const rapidjson::Value &member) {
            if (name != "techniques" && name != "properties") {
                return;
            }
            if (!member.IsArray()) {
                value.version = 0;
                return;
            }
            for (const auto &item : member.GetArray()) {
                if (!item.IsObject()) {
                    value.version = 0;
                } else if (name == "techniques") {
                    MaterialTechniqueData technique;
                    if (!LoadTechniqueJson(item, technique)) {
                        value.version = 0;
                    }
                    value.techniques.push_back(std::move(technique));
                } else {
                    MaterialPropertyData property;
                    LoadPropertyEntryJson(item, property);
                    value.properties.push_back(std::move(property));
                }
            }
        });
        if (value.version == 0) {
            value.clear();
        }
    }

    void JsonSaveMaterialAssetData(const MaterialAssetData &value, sky::JsonOutputArchive &archive)
    {
        archive.StartObject();
        archive.Key("techniques");
        archive.StartArray();
        for (const auto &t : value.techniques) {
            SaveTechniqueJson(archive, t);
        }
        archive.EndArray();
        archive.Key("properties");
        archive.StartArray();
        for (const auto &p : value.properties) {
            SavePropertyEntryJson(archive, p);
        }
        archive.EndArray();
        archive.EndObject();
    }

    void JsonLoadMaterialInstanceSource(MaterialInstanceSource &value, sky::JsonInputArchive &archive)
    {
        archive.ForEachMember([&value](const std::string &name, const rapidjson::Value &member) {
            if (name == "material" && member.IsString()) {
                value.material = Uuid::CreateFromString(member.GetString());
            } else if (name == "properties" && member.IsArray()) {
                for (const auto &p : member.GetArray()) {
                    if (p.IsObject()) {
                        MaterialPropertyData property;
                        LoadPropertyEntryJson(p, property);
                        value.properties.push_back(std::move(property));
                    }
                }
            }
        });
    }

    void JsonSaveMaterialInstanceSource(const MaterialInstanceSource &value, sky::JsonOutputArchive &archive)
    {
        archive.StartObject();
        archive.Key("material");
        archive.SaveValue(value.material.ToString());
        archive.Key("properties");
        archive.StartArray();
        for (const auto &p : value.properties) {
            SavePropertyEntryJson(archive, p);
        }
        archive.EndArray();
        archive.EndObject();
    }

    std::unique_ptr<sky::TypeNode> MakeMaterialPropertyReflection(const MaterialPropertyData &property)
    {
        if (!MaterialPropertyValueMatches(property.type, property.value))
            return {};
        auto node = std::make_unique<TypeNode>();
        std::visit(
            [&](const auto &value) {
                using T = std::decay_t<decltype(value)>;
                serialize::TypeMemberNode member{TypeInfoObj<T>::Get()->RtInfo(), false, false};
                member.setterFn = [](void *object, const void *value) {
                    auto       &property = *static_cast<MaterialPropertyData *>(object);
                    const auto &typed    = *static_cast<const T *>(value);
                    if constexpr (std::is_same_v<T, Sampler::Descriptor>)
                        if (!IsValidMaterialSampler(typed))
                            return false;
                    MaterialPropertyValue candidate = typed;
                    if (!MaterialPropertyValueMatches(property.type, candidate))
                        return false;
                    property.value = std::move(candidate);
                    return true;
                };
                member.getterConstFn = [](const void *object) {
                    const auto *value = std::get_if<T>(&static_cast<const MaterialPropertyData *>(object)->value);
                    return value ? Any(*value) : Any{};
                };
                if constexpr (std::is_same_v<T, Uuid>)
                    member.properties.emplace(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string("AuroraTexture")));
                node->members.emplace(property.name, std::move(member));
            },
            property.value);
        return node;
    }

    void RegisterMaterialReflection(sky::SerializationContext *context)
    {
        context->Register<Filter>("AuroraSamplerFilter").Enum(Filter::NEAREST, "Nearest").Enum(Filter::LINEAR, "Linear");
        context->Register<MipFilter>("AuroraSamplerMipFilter").Enum(MipFilter::NEAREST, "Nearest").Enum(MipFilter::LINEAR, "Linear");
        context->Register<WrapMode>("AuroraSamplerWrapMode")
            .Enum(WrapMode::REPEAT, "Repeat")
            .Enum(WrapMode::MIRRORED_REPEAT, "Mirrored Repeat")
            .Enum(WrapMode::CLAMP_TO_EDGE, "Clamp To Edge")
            .Enum(WrapMode::CLAMP_TO_BORDER, "Clamp To Border")
            .Enum(WrapMode::MIRROR_CLAMP_TO_EDGE, "Mirror Clamp To Edge");
        context->Register<Sampler::Descriptor>("AuroraSamplerDescriptor")
            .Member<&Sampler::Descriptor::magFilter>("magFilter")
            .Member<&Sampler::Descriptor::minFilter>("minFilter")
            .Member<&Sampler::Descriptor::mipmapMode>("mipmapMode")
            .Member<&Sampler::Descriptor::addressModeU>("addressModeU")
            .Member<&Sampler::Descriptor::addressModeV>("addressModeV")
            .Member<&Sampler::Descriptor::addressModeW>("addressModeW")
            .Member<&Sampler::Descriptor::minLod>("minLod")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.f))
            .Member<&Sampler::Descriptor::maxLod>("maxLod")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.f))
            .Member<&Sampler::Descriptor::maxAnisotropy>("maxAnisotropy")
            .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(1.f))
            .Member<&Sampler::Descriptor::anisotropyEnable>("anisotropyEnable");

        context->Register<MaterialAssetData>("MaterialAssetData")
            .BinLoad<&MaterialAssetData::Load>()
            .BinSave<&MaterialAssetData::Save>()
            .JsonLoad<&JsonLoadMaterialAssetData>()
            .JsonSave<&JsonSaveMaterialAssetData>();

        context->Register<MaterialInstanceSource>("MaterialInstanceSource")
            .JsonLoad<&JsonLoadMaterialInstanceSource>()
            .JsonSave<&JsonSaveMaterialInstanceSource>();
    }

} // namespace sky::aurora
