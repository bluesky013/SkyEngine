//
// Created on 2026/10/05.
//

#include <framework/project/ProjectDescriptor.h>

#include <framework/platform/PlatformBase.h>

#include <rapidjson/document.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

static const char *TAG = "Project";

namespace sky {

    namespace {
        std::filesystem::path AsPath(const std::string &value)
        {
            return std::filesystem::path(value);
        }

        bool ReadAll(const std::string &path, std::string &out)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                return false;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            out = ss.str();
            return true;
        }

        bool WriteAll(const std::string &path, const std::string &data)
        {
            const auto parent = AsPath(path).parent_path();
            if (!parent.empty()) {
                std::error_code error;
                std::filesystem::create_directories(parent, error);
            }
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            return static_cast<bool>(out);
        }

        std::string Escape(const std::string &value)
        {
            std::string out;
            out.reserve(value.size() + 8);
            for (const char ch : value) {
                switch (ch) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += ch; break;
                }
            }
            return out;
        }

        std::string Quote(const std::string &value)
        {
            return "\"" + Escape(value) + "\"";
        }
    } // namespace

    std::string ProjectDescriptor::Dir() const
    {
        return AsPath(filePath).parent_path().string();
    }

    std::string ProjectDescriptor::AssetsDir() const
    {
        return (AsPath(Dir()) / "assets").string();
    }

    std::string ProjectDescriptor::ConfigsDir() const
    {
        return (AsPath(Dir()) / "configs").string();
    }

    std::string ProjectDescriptor::CacheDir() const
    {
        return (AsPath(Dir()) / "cache").string();
    }

    std::string ProjectDescriptor::MakeId()
    {
        static std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        const uint64_t a = rng();
        const uint64_t b = rng();
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%016llx%08llx", static_cast<unsigned long long>(a),
                      static_cast<unsigned long long>(b & 0xffffffffULL));
        return buffer;
    }

    bool ProjectDescriptor::Read(const std::string &skyprojPath)
    {
        std::string json;
        if (!ReadAll(skyprojPath, json)) {
            return false;
        }
        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError() || !document.IsObject()) {
            return false;
        }

        filePath = skyprojPath;
        auto str = [&document](const char *key) -> std::string {
            if (document.HasMember(key) && document[key].IsString()) {
                return document[key].GetString();
            }
            return {};
        };
        id            = str("id");
        name          = str("name");
        engineVersion = str("engineVersion");
        defaultScene  = str("defaultScene");

        modules.clear();
        if (document.HasMember("modules") && document["modules"].IsArray()) {
            for (const auto &m : document["modules"].GetArray()) {
                if (m.IsString()) {
                    modules.emplace_back(m.GetString());
                } else if (m.IsObject() && m.HasMember("id") && m["id"].IsString()) {
                    modules.emplace_back(m["id"].GetString());
                }
            }
        }
        plugins.clear();
        if (document.HasMember("plugins") && document["plugins"].IsArray()) {
            for (const auto &p : document["plugins"].GetArray()) {
                if (p.IsString()) {
                    plugins.emplace_back(p.GetString());
                } else if (p.IsObject() && p.HasMember("id") && p["id"].IsString()) {
                    plugins.emplace_back(p["id"].GetString());
                }
            }
        }
        return true;
    }

    bool ProjectDescriptor::Write(const std::string &skyprojPath) const
    {
        std::string json = "{\n";
        json += "  \"id\": " + Quote(id) + ",\n";
        json += "  \"name\": " + Quote(name) + ",\n";
        json += "  \"engineVersion\": " + Quote(engineVersion) + ",\n";
        json += "  \"defaultScene\": " + Quote(defaultScene) + ",\n";
        json += "  \"settings\": { \"rhi\": \"vulkan\" },\n";
        json += "  \"modules\": [";
        for (size_t i = 0; i < modules.size(); ++i) {
            json += (i == 0 ? "" : ", ") + Quote(modules[i]);
        }
        json += "],\n  \"plugins\": [";
        for (size_t i = 0; i < plugins.size(); ++i) {
            json += (i == 0 ? "" : ", ") + Quote(plugins[i]);
        }
        json += "]\n}\n";
        return WriteAll(skyprojPath, json);
    }

    bool ProjectDescriptor::Create(const std::string &dir, const std::string &name, std::string &skyprojPath)
    {
        const auto root = AsPath(dir) / name;
        std::error_code error;
        std::filesystem::create_directories(root / "assets", error);
        std::filesystem::create_directories(root / "configs", error);
        std::filesystem::create_directories(root / "cache", error);
        if (error) {
            return false;
        }

        ProjectDescriptor descriptor;
        descriptor.id            = MakeId();
        descriptor.name          = name;
        descriptor.engineVersion = kEngineVersion;
        descriptor.filePath      = (root / (name + ".skyproj")).string();
        if (!descriptor.Write(descriptor.filePath)) {
            return false;
        }
        skyprojPath = descriptor.filePath;
        return true;
    }

    std::string ProjectRegistry::FilePath() const
    {
        std::string base = Platform::Get() != nullptr ? Platform::Get()->GetUserConfigPath() : std::string{};
        if (base.empty()) {
            base = ".";
        }
        return (AsPath(base) / "skyengine" / "projects.json").string();
    }

    void ProjectRegistry::Load()
    {
        recent.clear();
        std::string json;
        if (!ReadAll(FilePath(), json)) {
            return;
        }
        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("recent") ||
            !document["recent"].IsArray()) {
            return;
        }
        for (const auto &entry : document["recent"].GetArray()) {
            if (entry.IsString()) {
                recent.emplace_back(entry.GetString());
            }
        }
    }

    void ProjectRegistry::Save()
    {
        std::string json = "{\n  \"recent\": [";
        for (size_t i = 0; i < recent.size(); ++i) {
            json += (i == 0 ? "" : ", ") + Quote(recent[i]);
        }
        json += "]\n}\n";
        WriteAll(FilePath(), json);
    }

    void ProjectRegistry::Add(const std::string &skyprojPath)
    {
        const auto it = std::find(recent.begin(), recent.end(), skyprojPath);
        if (it != recent.end()) {
            recent.erase(it);
        }
        recent.insert(recent.begin(), skyprojPath);
        Save();
    }

    void ProjectRegistry::Remove(const std::string &skyprojPath)
    {
        const auto it = std::find(recent.begin(), recent.end(), skyprojPath);
        if (it != recent.end()) {
            recent.erase(it);
            Save();
        }
    }

} // namespace sky
