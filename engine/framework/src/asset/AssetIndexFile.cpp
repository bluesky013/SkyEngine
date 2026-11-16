//
// Created by blues on 2026/10/2.
//

#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/JsonLines.h>

#include <core/logger/Logger.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>

static const char *TAG = "AssetIndexFile";

namespace sky {

    namespace {

        std::string ValueToString(const rapidjson::Value &value)
        {
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            value.Accept(writer);
            return std::string(buffer.GetString(), buffer.GetSize());
        }

    } // namespace

    std::string MakeCanonicalPath(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    AssetIndexFile::AssetIndexFile(std::string keyField_, std::string extraField_)
        : keyField(std::move(keyField_)), extraField(std::move(extraField_))
    {
    }

    void AssetIndexFile::Parse(const std::string &text)
    {
        ParsePartial(text);
    }

    FileOperationStatus AssetIndexFile::ParseChecked(const std::string &text)
    {
        AssetIndexFile candidate(keyField, extraField);
        candidate.ParsePartial(text);
        if (!candidate.IsComplete())
            return {FileOperationError::IOError, candidate.Errors().front().message};
        *this = std::move(candidate);
        return {};
    }

    void AssetIndexFile::ParsePartial(const std::string &text)
    {
        entries.clear();
        errors.clear();
        std::unordered_map<std::string, size_t> keys;
        std::unordered_map<Uuid, size_t>        identities;
        std::vector<size_t>                     lines;
        std::vector<bool>                       conflicting;
        size_t                                  begin      = 0;
        size_t                                  lineNumber = 0;
        while (begin < text.size()) {
            ++lineNumber;
            const auto             end = text.find('\n', begin);
            const std::string_view line(text.data() + begin, (end == std::string::npos ? text.size() : end) - begin);
            begin = end == std::string::npos ? text.size() : end + 1;
            if (line.find_first_not_of(" \t\r") == std::string_view::npos)
                continue;
            const auto          reject = [&](const char *reason) { errors.push_back({lineNumber, reason}); };
            rapidjson::Document doc;
            doc.Parse(line.data(), line.size());
            if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember(keyField.c_str()) || !doc[keyField.c_str()].IsString() ||
                !doc.HasMember("id") || !doc["id"].IsString()) {
                reject("Index contains a malformed entry");
                continue;
            }
            IndexFileEntry entry;
            entry.key.assign(doc[keyField.c_str()].GetString(), doc[keyField.c_str()].GetStringLength());
            const std::string_view id(doc["id"].GetString(), doc["id"].GetStringLength());
            if (id.size() != 36 || id[8] != '-' || id[13] != '-' || id[18] != '-' || id[23] != '-' || !(entry.id = Uuid::CreateFromString(id))) {
                reject("Index contains an invalid asset identity");
                continue;
            }
            const std::filesystem::path path(entry.key);
            if (entry.key.empty() || entry.key.find_first_of("\\:") != std::string::npos || entry.key.find('\0') != std::string::npos ||
                path.is_absolute() || path.lexically_normal().generic_string() != entry.key ||
                (keyField == "file" && entry.key.find('/') != std::string::npos) ||
                std::any_of(path.begin(), path.end(), [](const auto &part) { return part == ".." || part == "."; })) {
                reject("Index contains an invalid logical path");
                continue;
            }
            if (!extraField.empty() && doc.HasMember(extraField.c_str()) && !doc[extraField.c_str()].IsNull())
                entry.extra = ValueToString(doc[extraField.c_str()]);
            if (doc.HasMember("generated") && !doc["generated"].IsNull())
                entry.generated = ValueToString(doc["generated"]);
            const size_t index = entries.size();
            conflicting.push_back(false);
            lines.push_back(lineNumber);
            const auto conflict = [&](size_t previous) {
                if (!conflicting[previous])
                    errors.push_back({lines[previous], "Index contains a duplicate key or identity"});
                conflicting[previous] = true;
                if (!conflicting[index])
                    reject("Index contains a duplicate key or identity");
                conflicting[index] = true;
            };
            const auto [key, newKey] = keys.emplace(entry.key, index);
            if (!newKey)
                conflict(key->second);
            const auto [identity, newIdentity] = identities.emplace(entry.id, index);
            if (!newIdentity)
                conflict(identity->second);
            entries.push_back(std::move(entry));
        }
        size_t output = 0;
        for (size_t i = 0; i < entries.size(); ++i)
            if (!conflicting[i]) {
                if (output != i)
                    entries[output] = std::move(entries[i]);
                ++output;
            }
        entries.resize(output);
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.key < b.key; });
        std::stable_sort(errors.begin(), errors.end(), [](const auto &a, const auto &b) { return a.line < b.line; });
    }

    std::string AssetIndexFile::Serialize() const
    {
        std::string out;
        for (const auto &entry : entries) {
            rapidjson::StringBuffer                    buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.StartObject();

            writer.Key(keyField.c_str());
            writer.String(entry.key.data(), static_cast<rapidjson::SizeType>(entry.key.size()));

            writer.Key("id");
            auto idStr = entry.id.ToString();
            writer.String(idStr.c_str());

            if (!extraField.empty() && !entry.extra.empty()) {
                rapidjson::Document extra;
                extra.Parse(entry.extra.c_str(), entry.extra.size());
                if (!extra.HasParseError()) {
                    writer.Key(extraField.c_str());
                    extra.Accept(writer);
                }
            }

            if (!entry.generated.empty()) {
                rapidjson::Document generated;
                generated.Parse(entry.generated.c_str(), entry.generated.size());
                if (!generated.HasParseError()) {
                    writer.Key("generated");
                    generated.Accept(writer);
                }
            }
            writer.EndObject();
            out.append(buffer.GetString(), buffer.GetSize());
            out.push_back('\n');
        }
        return out;
    }

    const IndexFileEntry *AssetIndexFile::Find(std::string_view key) const
    {
        const auto found = std::lower_bound(entries.begin(), entries.end(), key, [](const auto &entry, auto value) { return entry.key < value; });
        return found != entries.end() && found->key == key ? &*found : nullptr;
    }

    void AssetIndexFile::Set(const IndexFileEntry &entry)
    {
        const auto found =
            std::lower_bound(entries.begin(), entries.end(), entry.key, [](const auto &old, const auto &key) { return old.key < key; });
        if (found != entries.end() && found->key == entry.key)
            *found = entry;
        else
            entries.insert(found, entry);
    }

    void AssetIndexFile::SetEntries(std::span<const IndexFileEntry> values)
    {
        std::unordered_map<std::string, IndexFileEntry> merged;
        merged.reserve(entries.size() + values.size());
        for (const auto &entry : entries)
            merged.emplace(entry.key, entry);
        for (const auto &entry : values)
            merged.insert_or_assign(entry.key, entry);
        entries.clear();
        entries.reserve(merged.size());
        for (auto &[key, entry] : merged)
            entries.push_back(std::move(entry));
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.key < b.key; });
    }

    bool AssetIndexFile::Remove(std::string_view key)
    {
        auto iter = std::remove_if(entries.begin(), entries.end(), [key](const IndexFileEntry &entry) { return std::string_view(entry.key) == key; });
        if (iter == entries.end()) {
            return false;
        }
        entries.erase(iter, entries.end());
        return true;
    }

    AssetIndexFile AssetIndexFile::Load(const FileSystemPtr &fs, const FilePath &path, std::string keyField, std::string extraField)
    {
        AssetIndexFile file(std::move(keyField), std::move(extraField));
        if (fs == nullptr) {
            return file;
        }

        auto f = fs->OpenFile(path);
        if (f == nullptr) {
            if (fs->FileExist(path))
                file.errors.push_back({0, "Existing index could not be opened"});
            return file;
        }

        std::string text;
        if (!f->ReadString(text)) {
            file.errors.push_back({0, "Existing index could not be read"});
            return file;
        }

        file.Parse(text);
        return file;
    }

    FileOperationStatus AssetIndexFile::LoadChecked(const FileSystemPtr &fs, const FilePath &path, AssetIndexFile &out)
    {
        if (!fs)
            return {FileOperationError::InvalidPath, "Manifest filesystem is unavailable"};
        AssetIndexFile loaded(out.keyField, out.extraField);
        if (fs->FileExist(path)) {
            const auto  file = fs->OpenFile(path);
            std::string text;
            if (!file || !file->ReadString(text))
                return {FileOperationError::IOError, "Existing manifest could not be read"};
            if (const auto status = loaded.ParseChecked(text); !status)
                return status;
        }
        out = std::move(loaded);
        return {};
    }

    AssetFileWriteResult AssetIndexFile::Save(const FileSystemPtr &fs, const FilePath &path, const AssetIndexFile &file)
    {
        if (!file.IsComplete())
            return {{FileOperationError::InvalidPath, "Incomplete index cannot be saved"}, {}};
        return SaveAtomic(fs, path, file.Serialize());
    }

    AssetIndexFileCache::AssetIndexFileCache(std::string fileName_, std::string keyField_, std::string extraField_)
        : fileName(std::move(fileName_)), keyField(std::move(keyField_)), extraField(std::move(extraField_))
    {
    }

    std::shared_ptr<AssetIndexFileCache::Slot> AssetIndexFileCache::GetSlot(const FileSystemPtr &fs, const FilePath &dir)
    {
        std::lock_guard lock(mutex);
        auto           &slot = files[fs.Get()][std::filesystem::path(dir.GetStr()).lexically_normal().generic_string()];
        if (!slot)
            slot = std::make_shared<Slot>(fs);
        return slot;
    }

    AssetIndexFileCache::Snapshot AssetIndexFileCache::Get(const FileSystemPtr &fs, const FilePath &dir)
    {
        const auto      slot = GetSlot(fs, dir);
        std::lock_guard lock(slot->mutex);
        if (!slot->snapshot)
            slot->snapshot = std::make_shared<const AssetIndexFile>(AssetIndexFile::Load(fs, dir / FilePath(fileName), keyField, extraField));
        return slot->snapshot;
    }

    FileOperationStatus AssetIndexFileCache::Reload(const FileSystemPtr &fs, const FilePath &dir, AssetIndexFile &out)
    {
        AssetIndexFile loaded(keyField, extraField);
        if (const auto status = AssetIndexFile::LoadChecked(fs, dir / FilePath(fileName), loaded); !status)
            return status;
        out = std::move(loaded);
        return {};
    }

    AssetFileWriteResult AssetIndexFileCache::Save(const FileSystemPtr &fs, const FilePath &dir, const AssetIndexFile &file)
    {
        const auto      slot = GetSlot(fs, dir);
        std::lock_guard lock(slot->mutex);
        const auto      result = AssetIndexFile::Save(fs, dir / FilePath(fileName), file);
        if (result)
            slot->snapshot = std::make_shared<const AssetIndexFile>(file);
        return result;
    }

    void AssetIndexFileCache::Invalidate(const FileSystemPtr &fs, const FilePath &dir)
    {
        const auto      slot = GetSlot(fs, dir);
        std::lock_guard lock(slot->mutex);
        slot->snapshot.reset();
    }

    void AssetIndexFileCache::Clear()
    {
        std::lock_guard lock(mutex);
        for (auto &[filesystem, directories] : files) {
            std::erase_if(directories, [](const auto &entry) {
                const auto &slot = entry.second;
                if (slot.use_count() == 1)
                    return true;
                // In-flight operations must keep their shared serialization point.
                std::lock_guard slotLock(slot->mutex);
                slot->snapshot.reset();
                return false;
            });
        }
        std::erase_if(files, [](const auto &entry) { return entry.second.empty(); });
    }

} // namespace sky
