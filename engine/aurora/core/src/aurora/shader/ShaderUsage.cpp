//
// ShaderUsage: collection + offline precompile.
//

#include <aurora/shader/ShaderUsage.h>

#include <aurora/shader/ShaderResolver.h>

#include <core/file/FileSystem.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace sky::aurora {

    namespace {

        bool SameUsage(const ShaderUsageEntry &a, const ShaderUsageEntry &b)
        {
            return a.relativePath == b.relativePath && a.entry == b.entry && a.target == b.target && a.variantHash == b.variantHash;
        }

        void AppendEntry(rapidjson::Value &array, const ShaderUsageEntry &e, rapidjson::Document::AllocatorType &alloc)
        {
            rapidjson::Value obj(rapidjson::kObjectType);
            obj.AddMember("path", rapidjson::StringRef(e.relativePath.c_str()), alloc);
            obj.AddMember("entry", rapidjson::StringRef(e.entry.c_str()), alloc);
            obj.AddMember("stage", e.stage, alloc);
            obj.AddMember("target", e.target, alloc);
            obj.AddMember("variantHash", e.variantHash, alloc);
            obj.AddMember("variantWord0", e.variantKey[0], alloc);
            obj.AddMember("variantWord1", e.variantKey[1], alloc);
            obj.AddMember("variantDump", rapidjson::StringRef(e.variantDump.c_str()), alloc);
            obj.AddMember("toolchainFp", e.toolchainFp, alloc);
            array.PushBack(obj, alloc);
        }

        bool ParseEntry(const rapidjson::Value &v, ShaderUsageEntry &out)
        {
            if (!v.IsObject() || !v.HasMember("path") || !v.HasMember("entry")) {
                return false;
            }
            out.relativePath  = v["path"].GetString();
            out.entry         = v["entry"].GetString();
            out.stage         = v.HasMember("stage") ? v["stage"].GetUint() : 0;
            out.target        = v.HasMember("target") ? v["target"].GetUint() : 0;
            out.variantHash   = v.HasMember("variantHash") ? v["variantHash"].GetUint64() : 0;
            out.variantKey[0] = v.HasMember("variantWord0") ? v["variantWord0"].GetUint64() : 0;
            out.variantKey[1] = v.HasMember("variantWord1") ? v["variantWord1"].GetUint64() : 0;
            out.variantDump   = v.HasMember("variantDump") ? v["variantDump"].GetString() : "";
            out.toolchainFp   = v.HasMember("toolchainFp") ? v["toolchainFp"].GetUint64() : 0;
            return true;
        }

    } // namespace

    void ShaderUsageCollector::Record(const ShaderUsageEntry &entry)
    {
        if (!mEnabled) {
            return;
        }
        for (const auto &e : mEntries) {
            if (SameUsage(e, entry)) {
                return;
            }
        }
        mEntries.push_back(entry);
    }

    bool ShaderUsageCollector::Load(sky::IFileSystem &fs, const std::string &path, std::vector<ShaderUsageEntry> &out)
    {
        sky::FilePtr file = fs.OpenFile(sky::FilePath(path));
        if (file == nullptr) {
            return false;
        }
        std::string content;
        if (!file->ReadString(content)) {
            return false;
        }
        rapidjson::Document doc;
        if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject() || !doc.HasMember("entries") || !doc["entries"].IsArray()) {
            return false;
        }
        for (const auto &v : doc["entries"].GetArray()) {
            ShaderUsageEntry e;
            if (ParseEntry(v, e)) {
                out.push_back(std::move(e));
            }
        }
        return true;
    }

    bool ShaderUsageCollector::Flush(sky::IFileSystem &fs, const std::string &path)
    {
        std::vector<ShaderUsageEntry> merged;
        Load(fs, path, merged); // ignore absence
        for (const auto &e : mEntries) {
            bool found = false;
            for (const auto &m : merged) {
                if (SameUsage(m, e)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                merged.push_back(e);
            }
        }

        rapidjson::Document doc(rapidjson::kObjectType);
        auto               &alloc = doc.GetAllocator();
        rapidjson::Value    entries(rapidjson::kArrayType);
        for (const auto &e : merged) {
            AppendEntry(entries, e, alloc);
        }
        doc.AddMember("entries", entries, alloc);

        rapidjson::StringBuffer                    buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        sky::FilePtr file = fs.CreateOrOpenFile(sky::FilePath(path));
        if (file == nullptr) {
            return false;
        }
        const auto archive = file->WriteAsArchive();
        if (archive == nullptr) {
            return false;
        }
        return archive->SaveRaw(buffer.GetString(), buffer.GetSize());
    }

    std::vector<ShaderUsageGroup> GroupShaderUsage(const std::vector<ShaderUsageEntry> &usage)
    {
        std::vector<ShaderUsageGroup> groups;
        for (const auto &e : usage) {
            ShaderUsageGroup *group = nullptr;
            for (auto &g : groups) {
                if (g.relativePath == e.relativePath && g.target == e.target && g.variantHash == e.variantHash) {
                    group = &g;
                    break;
                }
            }
            if (group == nullptr) {
                groups.push_back(ShaderUsageGroup{e.relativePath, e.target, e.variantHash, {}});
                group = &groups.back();
            }
            bool dup = false;
            for (const auto &ge : group->entries) {
                if (ge.entry == e.entry && ge.stage == e.stage) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                group->entries.push_back(e);
            }
        }
        return groups;
    }

    bool BuildUsageCache(ShaderResolver &resolver, const std::vector<ShaderUsageEntry> &usage, std::string *error)
    {
        for (const auto &group : GroupShaderUsage(usage)) {
            for (const auto &e : group.entries) {
                ShaderResolver::Request req;
                req.relativePath = e.relativePath;
                req.entry        = e.entry;
                req.stage        = static_cast<ShaderStageFlagBit>(e.stage);
                req.target       = e.target;
                req.variantHash  = e.variantHash;
                // schema is resolved internally per path

                ShaderCompileResult result;
                std::string         localError;
                if (!resolver.ResolveShader(req, result, &localError)) {
                    if (error != nullptr) {
                        *error = group.relativePath + ":" + e.entry + " -> " + localError;
                    }
                    return false;
                }
            }
        }
        return true;
    }

} // namespace sky::aurora
