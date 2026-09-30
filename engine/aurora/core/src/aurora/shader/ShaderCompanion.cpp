//
// ShaderCompanion: parse a `.slang.json` companion into a variant schema.
//

#include <aurora/shader/ShaderCompanion.h>

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderHash.h>

#include <rapidjson/document.h>

#include <cstdint>
#include <unordered_map>

namespace sky::aurora {

    bool ParseShaderCompanion(std::string_view json, ShaderCompanion &out, std::string *error)
    {
        rapidjson::Document doc;
        if (doc.Parse(json.data(), json.size()).HasParseError()) {
            if (error != nullptr) {
                *error = "companion JSON parse error";
            }
            return false;
        }
        if (!doc.IsObject()) {
            if (error != nullptr) {
                *error = "companion root is not an object";
            }
            return false;
        }

        out = ShaderCompanion{};

        // sources: assign bit offsets sequentially by declared width
        std::unordered_map<std::string, uint16_t> sourceCursor;
        uint16_t                                  running = 0;
        if (doc.HasMember("sources") && doc["sources"].IsArray()) {
            for (const auto &src : doc["sources"].GetArray()) {
                if (!src.IsObject() || !src.HasMember("name")) {
                    continue;
                }
                const std::string name  = src["name"].GetString();
                const uint8_t     width = static_cast<uint8_t>(src["width"].GetUint());
                out.schema.sources.push_back({Name(name.c_str()), running, width});
                sourceCursor[name] = 0;
                running            = static_cast<uint16_t>(running + width);
            }
        }

        // variants -> schema entries (bit offsets relative to their source)
        if (doc.HasMember("variants") && doc["variants"].IsArray()) {
            for (const auto &v : doc["variants"].GetArray()) {
                if (!v.IsObject() || !v.HasMember("key") || !v.HasMember("source")) {
                    continue;
                }
                const std::string key    = v["key"].GetString();
                const std::string source = v["source"].GetString();
                const auto        it     = sourceCursor.find(source);
                if (it == sourceCursor.end()) {
                    if (error != nullptr) {
                        *error = "companion variant references unknown source: " + source;
                    }
                    return false;
                }
                const uint8_t width = v.HasMember("width") ? static_cast<uint8_t>(v["width"].GetUint()) : 1;

                ShaderVariantSchema::Entry entry{};
                entry.key          = Name(key.c_str());
                entry.source       = Name(source.c_str());
                entry.bitOffset    = it->second;
                entry.bitWidth     = width;
                entry.defaultValue = v.HasMember("default") ? v["default"].GetUint() : 0u;
                if (v.HasMember("spec")) {
                    entry.isSpec = true;
                    entry.specId = v["spec"].GetUint();
                }
                out.schema.entries.push_back(entry);

                it->second = static_cast<uint16_t>(it->second + width);
            }
        }
        out.schema.totalBits = running;

        // vertex switch definitions
        if (doc.HasMember("vertex") && doc["vertex"].IsArray()) {
            for (const auto &vd : doc["vertex"].GetArray()) {
                if (!vd.IsObject() || !vd.HasMember("name") || !vd.HasMember("semantics")) {
                    continue;
                }
                VertexVariantDef def;
                def.name = Name(vd["name"].GetString());
                for (const auto &sem : vd["semantics"].GetArray()) {
                    VertexSemantic parsed;
                    if (ParseVertexSemantic(sem.GetString(), parsed)) {
                        def.semantics.push_back(parsed);
                    }
                }
                out.vertexDefs.push_back(std::move(def));
            }
        }

        if (doc.HasMember("entries") && doc["entries"].IsArray()) {
            for (const auto &e : doc["entries"].GetArray()) {
                out.entryPoints.emplace_back(e.GetString());
            }
        }
        if (doc.HasMember("depends") && doc["depends"].IsArray()) {
            for (const auto &d : doc["depends"].GetArray()) {
                out.depends.emplace_back(d.GetString());
            }
        }

        std::string validateError;
        if (!out.schema.Validate(&validateError)) {
            if (error != nullptr) {
                *error = "companion schema invalid: " + validateError;
            }
            return false;
        }

        out.fingerprint = HashSchema(out.schema);
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
