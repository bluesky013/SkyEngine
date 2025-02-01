//
// Created by blues on 2024/12/31.
//

#include <shader/ShaderCacheManager.h>
#include <shader/ShaderFileSystem.h>
#include <core/archive/MemoryArchive.h>

namespace sky {

    ShaderCacheManager::ShaderCacheManager()
    {
        cacheEvent.Bind(this);
    }

    void ShaderCacheManager::OnShaderCacheSaved(ShaderCompileTarget target, const MD5 &md5, const ShaderCacheEntry& entry)
    {
        auto &mapping = cacheMapping[target];
        std::lock_guard<std::recursive_mutex> lock(mutex);
        mapping.cacheEntries[md5] = entry;
    }

    const ShaderSourceEntry* ShaderCacheManager::FetchSource(const Name &name, ShaderCompileTarget target)
    {
        auto &mapping = cacheMapping[target];
        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto iter = mapping.entries.find(name);
        if (iter != mapping.entries.end()) {
            return &(iter->second);
        }

        auto [rst, source] = ShaderFileSystem::Get()->LoadCacheSource(name);
        if (rst) {
            ShaderFileSystem::Get()->SaveCacheSource(name, source);

            MD5 sourceMD5 = MD5::CalculateMD5(source);
            ShaderVariantList list;
            std::vector<ShaderOptionItem> items = ShaderCompiler::Get()->PreProcess(source);
            for (auto &item : items) {
                list.AddOptionItem(item);
            }
            return SaveShader(name, target, sourceMD5, list);
        }

        return nullptr;
    }

    const ShaderSourceEntry* ShaderCacheManager::SaveShader(const Name& shader, ShaderCompileTarget target, const MD5& sourceMD5, const ShaderVariantList& list)
    {
        auto &mapping = cacheMapping[target];
        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto &sourceEntry = mapping.entries[shader];
        if (sourceEntry.sourceMD5 != sourceMD5) {
            sourceEntry.entries.clear();
            sourceEntry.sourceMD5 = sourceMD5;
            sourceEntry.options = list.GetOptionEntries();
        }
        return &sourceEntry;
    }

    const ShaderCacheEntry *ShaderCacheManager::FetchBinaryCache(const Name &shader, ShaderCompileTarget target, const Name &entry, ShaderVariantKey key)
    {
        auto &mapping = cacheMapping[target];
        ShaderCacheKey cacheKey = {entry, key};

        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto &sourceEntry = mapping.entries[shader];

        auto iter = sourceEntry.entries.find(cacheKey);
        if (iter != sourceEntry.entries.end()) {
            return &mapping.cacheEntries.at(iter->second);
        }
        return nullptr;
    }

    void ShaderCacheManager::SaveBinaryCache(const Name& shader, ShaderCompileTarget target,
        const Name& entry, const ShaderVariantKey &key, const ShaderBuildResult &result)
    {
        auto &mapping = cacheMapping[target];
        ShaderCacheKey cacheKey = {entry, key};

        // calculate binary md5
        CounterPtr<MemoryArchive> memPtr = new MemoryArchive();
        auto &memory = *memPtr;

        ShaderCompiler::SaveToMemory(memory, result);
        auto binMD5 = MD5::CalculateMD5(memory.Data(), memory.Size());

        // save cache bin
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            auto &sourceEntry = mapping.entries[shader];
            sourceEntry.entries[cacheKey] = binMD5;
        }

        // check if md5 binary exists
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            auto iter = mapping.cacheEntries.find(binMD5);
            if (iter != mapping.cacheEntries.end()) {
                return;
            }
        }

        ShaderFileSystem::Get()->AppendBinaryCache(shader, target, binMD5, memPtr);
    }

    void ShaderCacheManager::LoadMappingFile(ShaderCompileTarget target)
    {
        auto fs = ShaderFileSystem::Get()->GetCacheFS();
        FilePath path(ShaderCompiler::GetTargetName(target).GetStr().data());
        path /= "shader_cache.bin";

        auto file = fs->OpenFile(path);
        if (!file) {
            return; // cache not exists.
        }

        auto archive = file->ReadAsArchive();

        auto &mapping = cacheMapping[target];
        archive->Load(mapping.version);
        uint32_t number = 0;
        archive->Load(number);
        for (uint32_t i = 0; i < number; ++i) {
            std::string name;
            archive->Load(name);
            Name shaderName(name.c_str());

            ShaderSourceEntry sourceEntry = {};
            archive->LoadRaw(reinterpret_cast<char *>(&sourceEntry.sourceMD5), sizeof(MD5));

            uint32_t cacheNum = 0;
            archive->Load(cacheNum);

            sourceEntry.options.resize(cacheNum);
            for (uint32_t j = 0; j < cacheNum; ++j) {
                archive->Load(name);

                auto &opt = sourceEntry.options[j];
                opt.key = Name(name.c_str());
                archive->Load(opt.range.first);
                archive->Load(opt.range.second);
                archive->Load(opt.dft);
            }

            cacheNum = 0;
            archive->Load(cacheNum);
            for (uint32_t j = 0; j < cacheNum; ++j) {
                ShaderCacheKey key = {};
                MD5 data = {};

                archive->Load(name);
                key.entry = Name(name.c_str());
                archive->LoadRaw(reinterpret_cast<char *>(&key.key), sizeof(ShaderVariantKey));
                archive->LoadRaw(reinterpret_cast<char *>(&data), sizeof(MD5));

                sourceEntry.entries.emplace(key, data);
            }
            mapping.entries.emplace(shaderName, std::move(sourceEntry));
        }

        archive->Load(number);

        std::string name;
        for (uint32_t i = 0; i < number; ++i) {
            MD5 data = {};
            ShaderCacheEntry entry = {};

            archive->LoadRaw(reinterpret_cast<char *>(&data), sizeof(MD5));
            archive->Load(name);
            entry.savedFileName = Name(name.c_str());
            archive->Load(entry.offset);
            archive->Load(entry.length);

            mapping.cacheEntries[data] = entry;
        }
    }

    void ShaderCacheManager::SaveMappingFile() const
    {
        ShaderFileSystem::Get()->WaitSavingJobs();

        auto fs = ShaderFileSystem::Get()->GetCacheFS();
        for (const auto &[target, sourceMapping] : cacheMapping) {
            auto subFS = fs->CreateSubSystem(ShaderCompiler::GetTargetName(target).GetStr().data(), true);
            auto file = subFS->CreateOrOpenFile("shader_cache.bin");
            auto archive = file->WriteAsArchive();

            archive->Save(sourceMapping.version);
            archive->Save(static_cast<uint32_t>(sourceMapping.entries.size()));

            for (const auto &[shaderName, sourceEntry] : sourceMapping.entries) {
                archive->Save(std::string(shaderName.GetStr().data()));
                archive->SaveRaw(reinterpret_cast<const char *>(&sourceEntry.sourceMD5), sizeof(MD5));

                archive->Save(static_cast<uint32_t>(sourceEntry.options.size()));
                for (const auto &opt : sourceEntry.options) {
                    archive->Save(std::string(opt.key.GetStr().data()));
                    archive->Save(opt.range.first);
                    archive->Save(opt.range.second);
                    archive->Save(opt.dft);
                }

                archive->Save(static_cast<uint32_t>(sourceEntry.entries.size()));
                for (const auto &[cacheKey, binEntries] : sourceEntry.entries) {
                    archive->Save(std::string(cacheKey.entry.GetStr().data()));
                    archive->SaveRaw(reinterpret_cast<const char*>(&cacheKey.key), sizeof(ShaderVariantKey));
                    archive->SaveRaw(reinterpret_cast<const char *>(&binEntries), sizeof(MD5));
                }
            }

            archive->Save(static_cast<uint32_t>(sourceMapping.cacheEntries.size()));
            for (const auto &[md5, cacheEntry] : sourceMapping.cacheEntries) {
                archive->SaveRaw(reinterpret_cast<const char *>(&md5), sizeof(MD5));
                archive->Save(std::string(cacheEntry.savedFileName.GetStr().data()));
                archive->Save(cacheEntry.offset);
                archive->Save(cacheEntry.length);
            }
        }
    }

} // namespace sky