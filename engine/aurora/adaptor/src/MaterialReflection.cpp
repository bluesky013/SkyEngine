//
// Material reflection: hand-written JSON codecs for the material source (`MaterialAssetData` /
// `MaterialInstanceSource`) so `.mat` / `.mati` are (de)serialized in a concise authored schema,
// bypassing the reflection classId/elements wrapper. Registered on the types; cook + editor use
// LoadValueObject / SaveValueObject.
//

#include <aurora/adaptor/assets/MaterialReflection.h>

#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <core/environment/Once.h>
#include <framework/serialization/SerializationContext.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

namespace sky::aurora {

    namespace {

        void LoadPropertyValueJson(MaterialPropertyValue &out, uint32_t index, const rapidjson::Value &v)
        {
            if (index == 0) {
                out = v.IsNumber() ? static_cast<float>(v.GetDouble()) : 0.0f;
            } else if (index == 1 && v.IsArray() && v.Size() >= 2) {
                Vector2 x{};
                for (int i = 0; i < 2; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 2 && v.IsArray() && v.Size() >= 3) {
                Vector3 x{};
                for (int i = 0; i < 3; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 3 && v.IsArray() && v.Size() >= 4) {
                Vector4 x{};
                for (int i = 0; i < 4; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 4 && v.IsArray() && v.Size() >= 16) {
                Matrix4 x{};
                for (int i = 0; i < 16; ++i) {
                    x.v[i] = static_cast<float>(v[static_cast<rapidjson::SizeType>(i)].GetDouble());
                }
                out = x;
            } else if (index == 5) {
                out = v.IsInt() ? v.GetInt() : 0;
            } else if (index == 6) {
                out = v.IsUint() ? v.GetUint() : 0u;
            } else if (index == 7) {
                out = v.IsBool() ? v.GetBool() : false;
            } else if (index == 8) {
                out = v.IsString() ? Uuid::CreateFromString(v.GetString()) : Uuid{};
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

        void LoadTechniqueJson(const rapidjson::Value &v, MaterialTechniqueData &out)
        {
            if (v.HasMember("tag") && v["tag"].IsString()) {
                out.tag = v["tag"].GetString();
            }
            if (v.HasMember("shader")) {
                const rapidjson::Value &shader = v["shader"];
                if (shader.IsString()) {
                    out.shader.relativePath = shader.GetString();
                } else if (shader.IsObject()) {
                    if (shader.HasMember("relativePath") && shader["relativePath"].IsString()) {
                        out.shader.relativePath = shader["relativePath"].GetString();
                    }
                    if (shader.HasMember("entry") && shader["entry"].IsString()) {
                        out.shader.entry = shader["entry"].GetString();
                    }
                }
            }
            if (v.HasMember("entry") && v["entry"].IsString()) {
                out.shader.entry = v["entry"].GetString();
            }
            if (v.HasMember("renderState") && v["renderState"].IsObject()) {
                LoadRenderStateJson(v["renderState"], out.renderState);
            }
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
            ar.Key("shader");
            ar.StartObject();
            ar.Key("relativePath");
            ar.SaveValue(t.shader.relativePath);
            ar.Key("entry");
            ar.SaveValue(t.shader.entry);
            ar.EndObject();
            ar.Key("renderState");
            SaveRenderStateJson(ar, t.renderState);
            ar.EndObject();
        }

    } // namespace

    void JsonLoadMaterialAssetData(MaterialAssetData &value, sky::JsonInputArchive &archive)
    {
        value.version = MaterialAssetData::CURRENT_VERSION;
        archive.ForEachMember([&value](const std::string &name, const rapidjson::Value &member) {
            if (name == "techniques" && member.IsArray()) {
                for (const auto &t : member.GetArray()) {
                    if (t.IsObject()) {
                        MaterialTechniqueData technique;
                        LoadTechniqueJson(t, technique);
                        value.techniques.push_back(std::move(technique));
                    }
                }
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

    void RegisterMaterialReflection(sky::SerializationContext *context)
    {
        // Idempotent across callers / module DLLs (AuroraReflection + tests).
        if (context == nullptr || !sky::Once::Get()->Run("AuroraMaterialReflection")) {
            return;
        }

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
