//
// Created by blues on 2026/10/2.
//

#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/JsonLines.h>

#include <core/logger/Logger.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>

static const char *TAG = "AssetIndexFile";

namespace sky {

    namespace {

        std::string ValueToString(const rapidjson::Value &value)
        {
            rapidjson::StringBuffer buffer;
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
        : keyField(std::move(keyField_))
        , extraField(std::move(extraField_))
    {
    }

    void AssetIndexFile::Parse(const std::string &text)
    {
        entries.clear();

        for (const auto &line : SplitJsonLines(text)) {
            rapidjson::Document doc;
            doc.Parse(line.c_str(), line.size());
            if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember(keyField.c_str()) || !doc[keyField.c_str()].IsString() ||
                !doc.HasMember("id") || !doc["id"].IsString()) {
                LOG_W(TAG, "Skip malformed index line: %s", line.c_str());
                continue;
            }

            IndexFileEntry entry;
            entry.key = doc[keyField.c_str()].GetString();
            entry.id = Uuid::CreateFromString(doc["id"].GetString());
            if (!entry.id) {
                LOG_W(TAG, "Skip index line with invalid uuid: %s", line.c_str());
                continue;
            }
            if (!extraField.empty() && doc.HasMember(extraField.c_str()) && !doc[extraField.c_str()].IsNull()) {
                entry.extra = ValueToString(doc[extraField.c_str()]);
            }
            Set(entry);
        }
    }

    std::string AssetIndexFile::Serialize() const
    {
        std::string out;
        for (const auto &entry : entries) {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.StartObject();

            writer.Key(keyField.c_str());
            writer.String(entry.key.c_str());

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

            writer.EndObject();
            out.append(buffer.GetString(), buffer.GetSize());
            out.push_back('\n');
        }
        return out;
    }

    const IndexFileEntry *AssetIndexFile::Find(std::string_view key) const
    {
        for (const auto &entry : entries) {
            if (std::string_view(entry.key) == key) {
                return &entry;
            }
        }
        return nullptr;
    }

    void AssetIndexFile::Set(const IndexFileEntry &entry)
    {
        for (auto &existing : entries) {
            if (existing.key == entry.key) {
                existing = entry;
                return;
            }
        }

        entries.emplace_back(entry);
        std::sort(entries.begin(), entries.end(), [](const IndexFileEntry &a, const IndexFileEntry &b) {
            return a.key < b.key;
        });
    }

    bool AssetIndexFile::Remove(std::string_view key)
    {
        auto iter = std::remove_if(entries.begin(), entries.end(), [key](const IndexFileEntry &entry) {
            return std::string_view(entry.key) == key;
        });
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
            return file;
        }

        std::string text;
        if (!f->ReadString(text)) {
            return file;
        }

        file.Parse(text);
        return file;
    }

    bool AssetIndexFile::Save(const FileSystemPtr &fs, const FilePath &path, const AssetIndexFile &file)
    {
        return SaveAtomic(fs, path, file.Serialize());
    }

    AssetIndexFileCache::AssetIndexFileCache(std::string fileName_, std::string keyField_, std::string extraField_)
        : fileName(std::move(fileName_))
        , keyField(std::move(keyField_))
        , extraField(std::move(extraField_))
    {
    }

    std::string AssetIndexFileCache::MakeKey(const FileSystemPtr &fs, const FilePath &dir) const
    {
        std::string key = fs != nullptr ? fs->GetPath().GetStr() : std::string();
        key.push_back('|');
        key += dir.GetStr();
        return key;
    }

    const AssetIndexFile &AssetIndexFileCache::Get(const FileSystemPtr &fs, const FilePath &dir)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto key = MakeKey(fs, dir);
        auto iter = files.find(key);
        if (iter == files.end()) {
            iter = files.emplace(key, AssetIndexFile::Load(fs, dir / FilePath(fileName), keyField, extraField)).first;
        }
        return iter->second;
    }

    bool AssetIndexFileCache::Save(const FileSystemPtr &fs, const FilePath &dir, const AssetIndexFile &file)
    {
        if (!AssetIndexFile::Save(fs, dir / FilePath(fileName), file)) {
            return false;
        }
        Set(fs, dir, file);
        return true;
    }

    void AssetIndexFileCache::Set(const FileSystemPtr &fs, const FilePath &dir, AssetIndexFile file)
    {
        std::lock_guard<std::mutex> lock(mutex);
        files.insert_or_assign(MakeKey(fs, dir), std::move(file));
    }

    void AssetIndexFileCache::Invalidate(const FileSystemPtr &fs, const FilePath &dir)
    {
        std::lock_guard<std::mutex> lock(mutex);
        files.erase(MakeKey(fs, dir));
    }

    void AssetIndexFileCache::Clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        files.clear();
    }

} // namespace sky
