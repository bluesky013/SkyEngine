#include <aurora/shader/ShaderSourceBundle.h>

#include <core/file/FileSystem.h>
#include <rapidjson/document.h>
#include <unordered_map>
#include <unordered_set>

namespace sky::aurora {
    bool IsShaderLogicalPath(const std::string &path)
    {
        if (path.empty() || path.front() == '/' || path.back() == '/') {
            return false;
        }
        for (const unsigned char character : path) {
            if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
                  character == '_' || character == '-' || character == '.' || character == '/')) {
                return false;
            }
        }
        size_t start = 0;
        while (start < path.size()) {
            const auto end     = path.find('/', start);
            const auto segment = path.substr(start, end == std::string::npos ? end : end - start);
            if (segment.empty() || segment == "." || segment == "..") {
                return false;
            }
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
        return true;
    }

    namespace {
        bool ReadPaths(const rapidjson::Value &object, const char *member, std::vector<std::string> &paths)
        {
            if (!object.HasMember(member) || !object[member].IsArray()) {
                return false;
            }
            for (const auto &value : object[member].GetArray()) {
                if (!value.IsString()) {
                    return false;
                }
                std::string path(value.GetString(), value.GetStringLength());
                if (!IsShaderLogicalPath(path)) {
                    return false;
                }
                paths.push_back(std::move(path));
            }
            return true;
        }
        bool ReadInventory(const rapidjson::Value &value, ShaderSourceBundle &bundle)
        {
            if (!value.IsObject() || !value.HasMember("owner") || !value["owner"].IsString()) {
                return false;
            }
            bundle.owner.assign(value["owner"].GetString(), value["owner"].GetStringLength());
            std::vector<std::string> files;
            if (!IsShaderLogicalPath(bundle.owner) || !ReadPaths(value, "dependencies", bundle.dependencies) ||
                !ReadPaths(value, "includeRoots", bundle.includeRoots) || !ReadPaths(value, "files", files)) {
                return false;
            }
            for (auto &path : files) {
                if (!path.starts_with(bundle.owner + "/")) {
                    return false;
                }
                bundle.files.push_back({std::move(path), {}});
            }
            return true;
        }
    } // namespace

    bool LoadShaderSourceBundles(sky::IFileSystem &root, const std::string &owner, std::vector<ShaderSourceBundle> &bundles, std::string *error)
    {
        const auto fail = [&](std::string message) {
            if (error != nullptr) {
                *error = std::move(message);
            }
            return false;
        };
        const auto  manifest = root.OpenFile("shader-sources.json");
        std::string text;
        if (!manifest || !manifest->ReadString(text)) {
            return fail("Shader source inventory is unavailable: shader-sources.json");
        }
        rapidjson::Document json;
        json.Parse(text.data(), text.size());
        if (json.HasParseError() || !json.IsObject() || !json.HasMember("version") || !json["version"].IsUint() || json["version"].GetUint() != 1 ||
            !json.HasMember("modules") || !json["modules"].IsArray()) {
            return fail("Invalid shader source inventory");
        }
        std::unordered_map<std::string, ShaderSourceBundle> inventories;
        std::unordered_set<std::string>                     ownerSpellings, fileSpellings;
        const auto                                          fold = [](std::string value) {
            for (auto &letter : value)
                if (letter >= 'A' && letter <= 'Z')
                    letter += 'a' - 'A';
            return value;
        };
        std::vector<std::string> pending;
        for (const auto &value : json["modules"].GetArray()) {
            ShaderSourceBundle bundle;
            if (!ReadInventory(value, bundle) || !ownerSpellings.insert(fold(bundle.owner)).second) {
                return fail("Invalid or duplicate shader module inventory");
            }
            for (const auto &file : bundle.files)
                if (!fileSpellings.insert(fold(file.path)).second)
                    return fail("Duplicate or case-colliding shader inventory path: " + file.path);
            if (owner.empty()) {
                pending.push_back(bundle.owner);
            }
            const auto key = bundle.owner;
            inventories.emplace(key, std::move(bundle));
        }
        if (!owner.empty()) {
            pending.push_back(owner);
        }
        std::vector<ShaderSourceBundle> staged;
        std::unordered_set<std::string> visited;
        while (!pending.empty()) {
            auto next = std::move(pending.back());
            pending.pop_back();
            if (!visited.insert(next).second) {
                continue;
            }
            auto found = inventories.find(next);
            if (found == inventories.end()) {
                return fail("Missing shader module dependency: " + next);
            }
            auto bundle = std::move(found->second);
            for (auto &file : bundle.files) {
                const auto input = root.OpenFile(file.path);
                if (!input || !input->ReadString(file.content)) {
                    return fail("Missing shader inventory asset: " + file.path);
                }
            }
            pending.insert(pending.end(), bundle.dependencies.begin(), bundle.dependencies.end());
            staged.push_back(std::move(bundle));
        }
        bundles = std::move(staged);
        return true;
    }
} // namespace sky::aurora
