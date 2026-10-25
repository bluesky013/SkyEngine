//
// ShaderCompanion: parse a `.slang.json` companion into a variant schema.
//

#include <aurora/shader/ShaderCompanion.h>

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderHash.h>

#include <rapidjson/document.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>

namespace sky::aurora {

    bool ParseShaderCompanion(std::string_view json, ShaderCompanion &out, std::string *error)
    {
        const auto fail = [&](const std::string &message) {
            if (error)
                *error = message;
            return false;
        };
        rapidjson::Document doc;
        if (json.size() > 16u * 1024u * 1024u || doc.Parse(json.data(), json.size()).HasParseError() || !doc.IsObject())
            return fail("Invalid companion JSON object");
        const auto unique = [&](auto &&self, const rapidjson::Value &value, uint32_t depth) -> bool {
            if (depth > 64)
                return false;
            if (value.IsObject()) {
                std::set<std::string> names;
                for (const auto &member : value.GetObject())
                    if (!names.emplace(member.name.GetString(), member.name.GetStringLength()).second || !self(self, member.value, depth + 1))
                        return false;
            } else if (value.IsArray()) {
                for (const auto &child : value.GetArray())
                    if (!self(self, child, depth + 1))
                        return false;
            } else if (value.IsString()) {
                return std::string_view(value.GetString(), value.GetStringLength()).find('\0') == std::string_view::npos;
            }
            return true;
        };
        if (!unique(unique, doc, 0))
            return fail("Duplicate companion member or invalid nested value");
        for (const char *key : {"sources", "variants", "vertex", "entries", "depends"})
            if (doc.HasMember(key) && !doc[key].IsArray())
                return fail(std::string("Companion field must be an array: ") + key);
        const auto stringField = [](const rapidjson::Value &object, const char *key) {
            return object.IsObject() && object.HasMember(key) && object[key].IsString() && object[key].GetStringLength() != 0;
        };
        ShaderCompanion                 candidate;
        std::map<std::string, uint32_t> cursors;
        uint32_t                        running = 0;
        if (doc.HasMember("sources"))
            for (const auto &source : doc["sources"].GetArray()) {
                if (!stringField(source, "name") || !source.HasMember("width") || !source["width"].IsUint() || source["width"].GetUint() == 0 ||
                    source["width"].GetUint() > ShaderVariantSchema::kMaxLogicalBits ||
                    running + source["width"].GetUint() > ShaderVariantSchema::kMaxLogicalBits ||
                    !cursors.emplace(source["name"].GetString(), 0).second)
                    return fail("Invalid or duplicate companion source");
                const auto width = source["width"].GetUint();
                candidate.schema.sources.push_back({Name(source["name"].GetString()), static_cast<uint16_t>(running), static_cast<uint16_t>(width)});
                running += width;
            }
        if (doc.HasMember("variants"))
            for (const auto &value : doc["variants"].GetArray()) {
                if (!stringField(value, "key") || !stringField(value, "source"))
                    return fail("Variant requires nonempty key and source names");
                if (value.HasMember("spec"))
                    return fail("Numeric specialization IDs are no longer authored; associate the source symbol instead");
                const auto source = cursors.find(value["source"].GetString());
                if (source == cursors.end())
                    return fail("Variant references an unknown source");
                if (value.HasMember("width") && (!value["width"].IsUint() || !value["width"].GetUint() || value["width"].GetUint() > 32))
                    return fail("Variant width must be in [1, 32]");
                ShaderVariantSchema::Entry entry;
                entry.key       = Name(value["key"].GetString());
                entry.source    = Name(value["source"].GetString());
                entry.bitOffset = static_cast<uint16_t>(source->second);
                entry.bitWidth  = value.HasMember("width") ? static_cast<uint8_t>(value["width"].GetUint()) : 1;
                if (value.HasMember("type")) {
                    if (!stringField(value, "type"))
                        return fail("Invalid variant type");
                    const std::string_view type(value["type"].GetString());
                    if (type == "bool")
                        entry.type = ShaderVariantType::BOOL;
                    else if (type == "int32")
                        entry.type = ShaderVariantType::INT32;
                    else if (type != "uint32")
                        return fail("Only bool/int32/uint32 variant values are supported");
                }
                if (value.HasMember("default")) {
                    const auto &d = value["default"];
                    if (entry.type == ShaderVariantType::BOOL && d.IsBool())
                        entry.defaultValue = d.GetBool() ? 1u : 0u;
                    else if (entry.type == ShaderVariantType::INT32 && d.IsInt())
                        entry.defaultValue = static_cast<uint32_t>(d.GetInt());
                    else if (entry.type == ShaderVariantType::UINT32 && d.IsUint())
                        entry.defaultValue = d.GetUint();
                    else
                        return fail("Variant default has the wrong scalar type or overflows");
                }
                if (value.HasMember("symbol")) {
                    if (!stringField(value, "symbol"))
                        return fail("Variant source symbol must be a nonempty string");
                    entry.symbol = Name(value["symbol"].GetString());
                    if (entry.symbol == entry.key)
                        return fail("Variant key must not replace its source symbol during preprocessing");
                }
                if (value.HasMember("domain")) {
                    if (!stringField(value, "domain"))
                        return fail("Invalid variant domain");
                    const std::string_view domain(value["domain"].GetString());
                    if (domain == "pipeline")
                        entry.domain = ShaderVariantDomain::PIPELINE;
                    else if (domain == "vertex")
                        entry.domain = ShaderVariantDomain::VERTEX;
                    else if (domain != "shader")
                        return fail("Variant domain must be shader, vertex or pipeline");
                }
                const auto *sourceDefinition = candidate.schema.FindSource(entry.source);
                if (source->second + entry.bitWidth > sourceDefinition->bitWidth)
                    return fail("Variant exceeds its source width");
                source->second += entry.bitWidth;
                candidate.schema.entries.push_back(entry);
            }
        candidate.schema.totalBits = running;
        std::set<Name> vertexKeys;
        if (doc.HasMember("vertex"))
            for (const auto &value : doc["vertex"].GetArray()) {
                if (!stringField(value, "name") || !value.HasMember("semantics") || !value["semantics"].IsArray() || value["semantics"].Empty())
                    return fail("Invalid vertex variant declaration");
                VertexVariantDef def{Name(value["name"].GetString())};
                auto             entry =
                    std::find_if(candidate.schema.entries.begin(), candidate.schema.entries.end(), [&](const auto &v) { return v.key == def.name; });
                if (entry == candidate.schema.entries.end() || entry->domain == ShaderVariantDomain::PIPELINE || !vertexKeys.insert(def.name).second)
                    return fail("Vertex switch requires a unique shader declaration");
                for (const auto &semantic : value["semantics"].GetArray()) {
                    VertexSemantic parsed;
                    if (!semantic.IsString() || !ParseVertexSemantic(semantic.GetString(), parsed))
                        return fail("Unknown vertex semantic");
                    def.semantics.push_back(parsed);
                    entry->vertexSemantics |= uint16_t(1u << static_cast<uint8_t>(parsed));
                }
                entry->domain = ShaderVariantDomain::VERTEX;
                candidate.vertexDefs.push_back(std::move(def));
            }
        for (const char *key : {"entries", "depends"})
            if (doc.HasMember(key)) {
                std::set<std::string> names;
                auto                 &output = std::string_view(key) == "entries" ? candidate.entryPoints : candidate.depends;
                for (const auto &value : doc[key].GetArray()) {
                    if (!value.IsString() || !value.GetStringLength() || !names.insert(value.GetString()).second)
                        return fail("Invalid or duplicate companion entry/dependency");
                    output.emplace_back(value.GetString());
                }
            }
        std::string validation;
        if (!candidate.schema.Validate(&validation))
            return fail("Invalid companion schema: " + validation);
        candidate.fingerprint = HashSchema(candidate.schema);
        out                   = std::move(candidate);
        return true;
    }

    bool LoadShaderCompanion(ShaderFileSystem &fs, const std::string &relativePath, ShaderCompanion &out, std::string *error)
    {
        std::string content;
        if (!fs.ReadFile(relativePath + ".json", content)) {
            if (error != nullptr) {
                *error = "companion not found: " + relativePath + ".json";
            }
            return false;
        }
        return ParseShaderCompanion(content, out, error);
    }

} // namespace sky::aurora
