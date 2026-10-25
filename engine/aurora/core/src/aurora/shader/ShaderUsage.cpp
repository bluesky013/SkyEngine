#include <aurora/shader/ShaderUsage.h>

#include "ShaderEnvironmentJson.h"

#include <aurora/shader/ShaderResolver.h>
#include <core/file/FileSystem.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <filesystem>
#include <string_view>

namespace sky::aurora {
    namespace {
        using Allocator = rapidjson::Document::AllocatorType;
        rapidjson::Value Text(std::string_view value, Allocator &allocator)
        {
            return rapidjson::Value(value.data(), static_cast<rapidjson::SizeType>(value.size()), allocator);
        }
        bool Fail(std::string *error, const std::string &message)
        {
            if (error != nullptr) {
                *error = message;
            }
            return false;
        }
        bool NormalizeUsage(ShaderUsageEntry &entry, std::string *error)
        {
            const auto stage = static_cast<ShaderStageFlagBit>(entry.stage);
            if (entry.relativePath.empty() || entry.entry.empty() || entry.relativePath.find('\0') != std::string::npos ||
                entry.entry.find('\0') != std::string::npos || entry.environment.profile.find('\0') != std::string::npos ||
                entry.target > static_cast<uint32_t>(ShaderTarget::DXIL) ||
                (stage != ShaderStageFlagBit::VS && stage != ShaderStageFlagBit::FS && stage != ShaderStageFlagBit::CS &&
                 stage != ShaderStageFlagBit::MS && stage != ShaderStageFlagBit::TAS) ||
                entry.environment.platform == ShaderPlatform::UNKNOWN || entry.environment.codeFormat == ShaderCodeFormat::UNKNOWN ||
                entry.environment.profile.empty() || entry.target != static_cast<uint32_t>(entry.environment.target)) {
                return Fail(error, "Shader usage requires an explicit complete target environment");
            }
            for (const auto &option : entry.environment.options) {
                if (option.empty() || option.find('\0') != std::string::npos) {
                    return Fail(error, "Invalid shader usage compiler option");
                }
            }
            std::string validation;
            if (!NormalizeShaderEnvironment(entry.environment, validation)) {
                return Fail(error, validation);
            }
            std::sort(entry.variant.entries.begin(), entry.variant.entries.end(),
                      [](const auto &a, const auto &b) { return a.key.GetStr() < b.key.GetStr(); });
            for (size_t i = 0; i < entry.variant.entries.size(); ++i) {
                if (entry.variant.entries[i].key.GetStr().empty() || (i && entry.variant.entries[i - 1].key == entry.variant.entries[i].key)) {
                    return Fail(error, "Duplicate or empty shader usage variant");
                }
            }
            // Keep a producer-supplied canonical hash (composed P|V|S); legacy raw
            // requirements without one are hashed from their material macros.
            if (entry.variantHash == 0) {
                entry.variantHash = entry.variant.ContentHash();
            }
            return true;
        }
        bool SameVariant(const ShaderVariant &left, const ShaderVariant &right)
        {
            if (left.entries.size() != right.entries.size()) {
                return false;
            }
            for (const auto &entry : left.entries) {
                if (std::none_of(right.entries.begin(), right.entries.end(),
                                 [&](const auto &other) { return entry.key == other.key && entry.value == other.value && entry.type == other.type; })) {
                    return false;
                }
            }
            return true;
        }
        bool SameUsage(const ShaderUsageEntry &left, const ShaderUsageEntry &right)
        {
            return left.relativePath == right.relativePath && left.entry == right.entry && left.stage == right.stage && left.target == right.target &&
                   left.environment == right.environment && SameVariant(left.variant, right.variant);
        }
        rapidjson::Value EnvironmentJson(const ShaderTargetEnvironment &environment, Allocator &allocator)
        {
            rapidjson::Value value(rapidjson::kObjectType);
            value.AddMember("target", static_cast<uint32_t>(environment.target), allocator);
            value.AddMember("platform", static_cast<uint32_t>(environment.platform), allocator);
            value.AddMember("codeFormat", static_cast<uint32_t>(environment.codeFormat), allocator);
            value.AddMember("profile", Text(environment.profile, allocator), allocator);
            value.AddMember("minimumVersion", environment.minimumVersion, allocator);
            value.AddMember("requiredCapabilities", environment.requiredCapabilities, allocator);
            value.AddMember("matrixLayout", static_cast<uint32_t>(environment.matrixLayout), allocator);
            rapidjson::Value options(rapidjson::kArrayType);
            for (const auto &option : environment.options) {
                options.PushBack(Text(option, allocator), allocator);
            }
            value.AddMember("options", options, allocator);
            return value;
        }
        void AppendEntry(rapidjson::Value &array, const ShaderUsageEntry &entry, Allocator &allocator)
        {
            rapidjson::Value value(rapidjson::kObjectType);
            value.AddMember("path", Text(entry.relativePath, allocator), allocator);
            value.AddMember("entry", Text(entry.entry, allocator), allocator);
            value.AddMember("stage", entry.stage, allocator);
            value.AddMember("target", entry.target, allocator);
            value.AddMember("variantHash", entry.variantHash, allocator);
            value.AddMember("variantWord0", entry.variantKey[0], allocator);
            value.AddMember("variantWord1", entry.variantKey[1], allocator);
            value.AddMember("variantDump", Text(entry.variantDump, allocator), allocator);
            value.AddMember("toolchainFp", entry.toolchainFp, allocator);
            value.AddMember("environment", EnvironmentJson(entry.environment, allocator), allocator);
            rapidjson::Value variant(rapidjson::kObjectType);
            for (const auto &item : entry.variant.entries) {
                auto key = Text(item.key.GetStr(), allocator);
                rapidjson::Value typed(rapidjson::kObjectType);
                typed.AddMember("type", static_cast<uint32_t>(item.type), allocator);
                typed.AddMember("value", item.value, allocator);
                variant.AddMember(key, typed, allocator);
            }
            value.AddMember("variant", variant, allocator);
            array.PushBack(value, allocator);
        }
        bool ParseEntry(const rapidjson::Value &value, ShaderUsageEntry &entry, std::string *error)
        {
            if (!value.IsObject() || !value.HasMember("path") || !value["path"].IsString() || !value.HasMember("entry") ||
                !value["entry"].IsString() || !value.HasMember("stage") || !value["stage"].IsUint() || !value.HasMember("target") ||
                !value["target"].IsUint() || !value.HasMember("environment") || !ReadShaderEnvironmentJson(value["environment"], entry.environment)) {
                return Fail(error, "Invalid shader usage entry or target environment");
            }
            entry.relativePath.assign(value["path"].GetString(), value["path"].GetStringLength());
            entry.entry.assign(value["entry"].GetString(), value["entry"].GetStringLength());
            entry.stage  = value["stage"].GetUint();
            entry.target = value["target"].GetUint();
            for (const char *key : {"variantHash", "variantWord0", "variantWord1", "toolchainFp"}) {
                if (value.HasMember(key) && !value[key].IsUint64()) {
                    return Fail(error, "Invalid shader usage integer");
                }
            }
            entry.variantHash   = value.HasMember("variantHash") ? value["variantHash"].GetUint64() : 0;
            entry.variantKey[0] = value.HasMember("variantWord0") ? value["variantWord0"].GetUint64() : 0;
            entry.variantKey[1] = value.HasMember("variantWord1") ? value["variantWord1"].GetUint64() : 0;
            entry.toolchainFp   = value.HasMember("toolchainFp") ? value["toolchainFp"].GetUint64() : 0;
            if (value.HasMember("variantDump")) {
                if (!value["variantDump"].IsString()) {
                    return Fail(error, "Invalid shader usage variant dump");
                }
                entry.variantDump = value["variantDump"].GetString();
            }
            if (value.HasMember("variant")) {
                if (!value["variant"].IsObject()) {
                    return Fail(error, "Invalid shader usage variants");
                }
                for (const auto &member : value["variant"].GetObject()) {
                    const std::string_view name(member.name.GetString(), member.name.GetStringLength());
                    if (name.find('\0') != std::string_view::npos)
                        return Fail(error, "Invalid shader usage variant name");
                    ShaderVariantEntry input{Name(member.name.GetString())};
                    if (member.value.IsUint()) {
                        input.value = member.value.GetUint();
                    } else {
                        const auto &typed = member.value;
                        if (!typed.IsObject() || typed.MemberCount() != 2 || !typed.HasMember("type") || !typed["type"].IsUint() ||
                            typed["type"].GetUint() > static_cast<uint32_t>(ShaderVariantType::BOOL) || !typed.HasMember("value") ||
                            !typed["value"].IsUint())
                            return Fail(error, "Invalid typed shader usage value");
                        input.type = static_cast<ShaderVariantType>(typed["type"].GetUint());
                        input.value = typed["value"].GetUint();
                    }
                    if (input.type == ShaderVariantType::BOOL && input.value > 1)
                        return Fail(error, "Invalid bool shader usage value");
                    entry.variant.entries.push_back(input);
                }
            }
            return NormalizeUsage(entry, error);
        }
    } // namespace
    bool ShaderUsageCollector::Record(const ShaderUsageEntry &entry, std::string *error)
    {
        if (!mEnabled) {
            return true;
        }
        auto normalized = entry;
        if (!NormalizeUsage(normalized, error)) {
            return false;
        }
        for (const auto &existing : mEntries) {
            if (SameUsage(existing, normalized)) {
                return true;
            }
        }
        mEntries.push_back(std::move(normalized));
        return true;
    }
    bool ShaderUsageCollector::Load(sky::IFileSystem &fs, const std::string &path, std::vector<ShaderUsageEntry> &out, std::string *error)
    {
        const auto  file = fs.OpenFile(sky::FilePath(path));
        std::string content;
        if (file == nullptr || !file->ReadString(content)) {
            return Fail(error, "Shader usage file cannot be read: " + path);
        }
        rapidjson::Document document;
        if (document.Parse(content.c_str()).HasParseError() || !document.IsObject() || !document.HasMember("formatVersion") ||
            !document["formatVersion"].IsUint() || document["formatVersion"].GetUint() != kFormatVersion || !document.HasMember("entries") ||
            !document["entries"].IsArray()) {
            return Fail(error, "Incompatible shader usage protocol; recook requirements or recollect runtime usage");
        }
        std::vector<ShaderUsageEntry> entries;
        for (const auto &value : document["entries"].GetArray()) {
            ShaderUsageEntry entry;
            if (!ParseEntry(value, entry, error)) {
                return false;
            }
            entries.push_back(std::move(entry));
        }
        out = std::move(entries);
        return true;
    }
    bool ShaderUsageCollector::Flush(sky::IFileSystem &fs, const std::string &path, std::string *error)
    {
        std::vector<ShaderUsageEntry> merged;
        if (fs.FileExist(sky::FilePath(path)) && !Load(fs, path, merged, error)) {
            return false;
        }
        for (const auto &entry : mEntries) {
            if (std::none_of(merged.begin(), merged.end(), [&](const auto &other) { return SameUsage(entry, other); })) {
                merged.push_back(entry);
            }
        }
        rapidjson::Document document(rapidjson::kObjectType);
        auto               &allocator = document.GetAllocator();
        rapidjson::Value    entries(rapidjson::kArrayType);
        for (const auto &entry : merged) {
            AppendEntry(entries, entry, allocator);
        }
        document.AddMember("formatVersion", kFormatVersion, allocator);
        document.AddMember("entries", entries, allocator);
        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);
        const std::vector<uint8_t> bytes(buffer.GetString(), buffer.GetString() + buffer.GetSize());
        if (!WriteFileAtomic(fs, path, bytes)) {
            return Fail(error, "Shader usage atomic publication failed");
        }
        return true;
    }
    std::vector<ShaderUsageGroup> GroupShaderUsage(const std::vector<ShaderUsageEntry> &usage)
    {
        std::vector<ShaderUsageGroup> groups;
        for (const auto &entry : usage) {
            auto found = std::find_if(groups.begin(), groups.end(), [&](const auto &group) {
                return group.relativePath == entry.relativePath && group.target == entry.target && group.environment == entry.environment &&
                       !group.entries.empty() && SameVariant(group.entries[0].variant, entry.variant);
            });
            if (found == groups.end()) {
                groups.push_back({entry.relativePath, entry.target, entry.variantHash, entry.environment, {entry}});
            } else if (std::none_of(found->entries.begin(), found->entries.end(), [&](const auto &other) { return SameUsage(entry, other); })) {
                found->entries.push_back(entry);
            }
        }
        return groups;
    }
    bool BuildUsageCache(ShaderResolver                   &resolver,
                         const std::vector<ShaderUsageEntry> &usage,
                         std::string                       *error,
                         const ShaderVariantSchema         *pipelineSchema)
    {
        for (const auto &group : GroupShaderUsage(usage)) {
            for (auto entry : group.entries) {
                if (!NormalizeUsage(entry, error)) {
                    return false;
                }
                ShaderResolver::Request request;
                request.relativePath = entry.relativePath;
                request.entry        = entry.entry;
                request.stage        = static_cast<ShaderStageFlagBit>(entry.stage);
                request.target       = entry.target;
                request.environment    = entry.environment;
                request.variant        = &entry.variant;
                request.pipelineSchema = pipelineSchema;
                ShaderCompileResult result;
                std::string         diagnostic;
                if (!resolver.ResolveShader(request, result, &diagnostic)) {
                    return Fail(error, entry.relativePath + ":" + entry.entry + " -> " + diagnostic);
                }
            }
        }
        return true;
    }

    bool ShaderUsageStore::Enabled()
    {
        auto *store = Get();
        std::lock_guard<std::mutex> lock(store->mMutex);
        return store->mCollector.Enabled();
    }

    void ShaderUsageStore::SetEnabled(bool enabled)
    {
        auto *store = Get();
        std::lock_guard<std::mutex> lock(store->mMutex);
        store->mCollector.SetEnabled(enabled);
    }

    void ShaderUsageStore::Record(const ShaderUsageEntry &entry)
    {
        auto *store = Get();
        std::lock_guard<std::mutex> lock(store->mMutex);
        if (store->mCollector.Enabled()) {
            store->mCollector.Record(entry);
        }
    }

    size_t ShaderUsageStore::Size()
    {
        auto *store = Get();
        std::lock_guard<std::mutex> lock(store->mMutex);
        return store->mCollector.Size();
    }

    std::vector<ShaderUsageEntry> ShaderUsageStore::Snapshot()
    {
        auto *store = Get();
        std::lock_guard<std::mutex> lock(store->mMutex);
        return store->mCollector.Entries();
    }

    void ShaderUsageStore::Clear()
    {
        auto *store = Get();
        std::lock_guard<std::mutex> lock(store->mMutex);
        const bool enabled = store->mCollector.Enabled();
        store->mCollector = ShaderUsageCollector{};
        store->mCollector.SetEnabled(enabled);
    }

    bool ShaderUsageStore::Flush(sky::IFileSystem &fs, const std::string &path)
    {
        auto *store = Get();
        std::lock_guard<std::mutex> lock(store->mMutex);
        return store->mCollector.Flush(fs, path);
    }

    bool ShaderUsageStore::Flush(const std::string &file)
    {
        const std::filesystem::path path(file);
        auto                        parent = path.parent_path().string();
        if (parent.empty()) {
            parent = ".";
        }
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        NativeFileSystem fs{FilePath(parent)};
        return Flush(fs, path.filename().string());
    }
} // namespace sky::aurora
