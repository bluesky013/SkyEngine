//
// Created on 2026/10/06.
//

#include <editor/core/preferences/PreferenceStore.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <utility>

namespace sky::editor {

    namespace {
        const char *TypeName(PreferenceType type)
        {
            switch (type) {
            case PreferenceType::BOOL: return "bool";
            case PreferenceType::INT: return "int";
            case PreferenceType::FLOAT: return "float";
            case PreferenceType::STRING: return "string";
            case PreferenceType::COLOR: return "color";
            }
            return "bool";
        }

        bool ParseType(const char *name, PreferenceType &out)
        {
            if (name == nullptr) {
                return false;
            }
            const std::string value(name);
            if (value == "bool") {
                out = PreferenceType::BOOL;
            } else if (value == "int") {
                out = PreferenceType::INT;
            } else if (value == "float") {
                out = PreferenceType::FLOAT;
            } else if (value == "string") {
                out = PreferenceType::STRING;
            } else if (value == "color") {
                out = PreferenceType::COLOR;
            } else {
                return false;
            }
            return true;
        }

        void WriteValue(rapidjson::Value &out, const PreferenceValue &value, rapidjson::Document::AllocatorType &allocator)
        {
            out.SetObject();
            out.AddMember("type", rapidjson::Value(TypeName(value.type), allocator), allocator);
            switch (value.type) {
            case PreferenceType::BOOL: out.AddMember("value", value.boolValue, allocator); break;
            case PreferenceType::INT: out.AddMember("value", value.intValue, allocator); break;
            case PreferenceType::FLOAT: out.AddMember("value", value.floatValue, allocator); break;
            case PreferenceType::STRING: out.AddMember("value", rapidjson::Value(value.stringValue.c_str(), allocator), allocator); break;
            case PreferenceType::COLOR: {
                rapidjson::Value array(rapidjson::kArrayType);
                array.PushBack(value.colorValue.r, allocator);
                array.PushBack(value.colorValue.g, allocator);
                array.PushBack(value.colorValue.b, allocator);
                array.PushBack(value.colorValue.a, allocator);
                out.AddMember("value", array, allocator);
                break;
            }
            }
        }

        PreferenceValue ReadValue(const rapidjson::Value &object)
        {
            PreferenceType type = PreferenceType::BOOL;
            ParseType(object.HasMember("type") && object["type"].IsString() ? object["type"].GetString() : "", type);
            const rapidjson::Value &value = object["value"];
            switch (type) {
            case PreferenceType::BOOL: return PreferenceValue::Bool(value.IsBool() ? value.GetBool() : false);
            case PreferenceType::INT: return PreferenceValue::Int(value.IsInt64() ? value.GetInt64() : 0);
            case PreferenceType::FLOAT: return PreferenceValue::Float(value.IsNumber() ? value.GetDouble() : 0.0);
            case PreferenceType::STRING: return PreferenceValue::Str(value.IsString() ? value.GetString() : "");
            case PreferenceType::COLOR: {
                PreferenceColor color;
                if (value.IsArray() && value.Size() == 4) {
                    color.r = value[0].GetFloat();
                    color.g = value[1].GetFloat();
                    color.b = value[2].GetFloat();
                    color.a = value[3].GetFloat();
                }
                return PreferenceValue::Color(color);
            }
            }
            return PreferenceValue::Bool(false);
        }
    } // namespace

    PreferenceStore::PreferenceStore(const PreferenceRegistry *inRegistry) : registry(inRegistry)
    {
        SeedDefaults();
    }

    void PreferenceStore::SetRegistry(const PreferenceRegistry *inRegistry)
    {
        registry = inRegistry;
        SeedDefaults();
    }

    void PreferenceStore::SeedDefaults()
    {
        if (registry == nullptr) {
            return;
        }
        for (const PreferencePage &page : registry->GetPages()) {
            for (const PreferenceSection &section : page.sections) {
                for (const PreferenceEntry &entry : section.entries) {
                    committed.emplace(entry.key, entry.defaultValue);
                    working.emplace(entry.key, entry.defaultValue);
                }
            }
        }
    }

    const PreferenceEntry *PreferenceStore::FindDeclared(const std::string &key) const
    {
        return registry != nullptr ? registry->FindEntry(key) : nullptr;
    }

    bool PreferenceStore::Assign(const std::string &key, PreferenceType type, PreferenceValue value)
    {
        const PreferenceEntry *declared = FindDeclared(key);
        if (declared == nullptr || declared->defaultValue.type != type) {
            return false;
        }
        working[key] = std::move(value);
        UpdateDirty();
        return true;
    }

    bool PreferenceStore::SetBool(const std::string &key, bool value)
    {
        return Assign(key, PreferenceType::BOOL, PreferenceValue::Bool(value));
    }

    bool PreferenceStore::SetInt(const std::string &key, int64_t value)
    {
        return Assign(key, PreferenceType::INT, PreferenceValue::Int(value));
    }

    bool PreferenceStore::SetFloat(const std::string &key, double value)
    {
        return Assign(key, PreferenceType::FLOAT, PreferenceValue::Float(value));
    }

    bool PreferenceStore::SetString(const std::string &key, std::string value)
    {
        return Assign(key, PreferenceType::STRING, PreferenceValue::Str(std::move(value)));
    }

    bool PreferenceStore::SetColor(const std::string &key, const PreferenceColor &value)
    {
        return Assign(key, PreferenceType::COLOR, PreferenceValue::Color(value));
    }

    bool PreferenceStore::GetBool(const std::string &key, bool &out) const
    {
        const auto it = working.find(key);
        if (it == working.end() || it->second.type != PreferenceType::BOOL) {
            return false;
        }
        out = it->second.boolValue;
        return true;
    }

    bool PreferenceStore::GetInt(const std::string &key, int64_t &out) const
    {
        const auto it = working.find(key);
        if (it == working.end() || it->second.type != PreferenceType::INT) {
            return false;
        }
        out = it->second.intValue;
        return true;
    }

    bool PreferenceStore::GetFloat(const std::string &key, double &out) const
    {
        const auto it = working.find(key);
        if (it == working.end() || it->second.type != PreferenceType::FLOAT) {
            return false;
        }
        out = it->second.floatValue;
        return true;
    }

    bool PreferenceStore::GetString(const std::string &key, std::string &out) const
    {
        const auto it = working.find(key);
        if (it == working.end() || it->second.type != PreferenceType::STRING) {
            return false;
        }
        out = it->second.stringValue;
        return true;
    }

    bool PreferenceStore::GetColor(const std::string &key, PreferenceColor &out) const
    {
        const auto it = working.find(key);
        if (it == working.end() || it->second.type != PreferenceType::COLOR) {
            return false;
        }
        out = it->second.colorValue;
        return true;
    }

    const PreferenceValue *PreferenceStore::FindWorking(const std::string &key) const
    {
        const auto it = working.find(key);
        return it != working.end() ? &it->second : nullptr;
    }

    const PreferenceValue *PreferenceStore::FindCommitted(const std::string &key) const
    {
        const auto it = committed.find(key);
        return it != committed.end() ? &it->second : nullptr;
    }

    void PreferenceStore::UpdateDirty()
    {
        dirty = false;
        for (const auto &[key, value] : working) {
            const auto it = committed.find(key);
            if (it == committed.end() || !value.Equals(it->second)) {
                dirty = true;
                return;
            }
        }
        for (const auto &[key, value] : committed) {
            if (working.find(key) == working.end()) {
                dirty = true;
                return;
            }
        }
    }

    void PreferenceStore::Commit()
    {
        committed = working;
        dirty     = false;
    }

    void PreferenceStore::Revert()
    {
        working = committed;
        dirty   = false;
    }

    void PreferenceStore::ResetAllToDefaults()
    {
        if (registry == nullptr) {
            return;
        }
        for (const PreferencePage &page : registry->GetPages()) {
            for (const PreferenceSection &section : page.sections) {
                for (const PreferenceEntry &entry : section.entries) {
                    working[entry.key] = entry.defaultValue;
                }
            }
        }
        UpdateDirty();
    }

    bool PreferenceStore::ResetPageToDefaults(const std::string &pageId)
    {
        if (registry == nullptr) {
            return false;
        }
        const PreferencePage *page = registry->FindPage(pageId);
        if (page == nullptr) {
            return false;
        }
        for (const PreferenceSection &section : page->sections) {
            for (const PreferenceEntry &entry : section.entries) {
                working[entry.key] = entry.defaultValue;
            }
        }
        UpdateDirty();
        return true;
    }

    std::vector<std::string> PreferenceStore::GetChangedKeys() const
    {
        std::vector<std::string> changed;
        for (const auto &[key, value] : working) {
            const auto it = committed.find(key);
            if (it == committed.end() || !value.Equals(it->second)) {
                changed.push_back(key);
            }
        }
        return changed;
    }

    std::string PreferenceStore::ToJson(int indent) const
    {
        rapidjson::Document document;
        document.SetObject();

        rapidjson::Document::AllocatorType &allocator = document.GetAllocator();
        document.AddMember("version", 1, allocator);

        rapidjson::Value values(rapidjson::kObjectType);
        for (const auto &[key, value] : committed) {
            rapidjson::Value entry(rapidjson::kObjectType);
            WriteValue(entry, value, allocator);
            values.AddMember(rapidjson::Value(key.c_str(), allocator), entry, allocator);
        }
        document.AddMember("values", values, allocator);

        rapidjson::StringBuffer                          buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetIndent(' ', static_cast<unsigned>(indent > 0 ? indent : 0));
        document.Accept(writer);
        return std::string(buffer.GetString());
    }

    bool PreferenceStore::FromJson(const std::string &json)
    {
        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError() || !document.IsObject()) {
            return false;
        }
        if (document.HasMember("values") && document["values"].IsObject()) {
            for (const auto &member : document["values"].GetObject()) {
                const std::string key = member.name.GetString();
                if (FindDeclared(key) == nullptr || !member.value.IsObject()) {
                    continue; // unknown/invalid key: skip (forward compatible)
                }
                PreferenceValue value = ReadValue(member.value);
                committed[key]        = value;
                working[key]          = value;
            }
        }
        SeedDefaults();
        UpdateDirty();
        return true;
    }

} // namespace sky::editor
