//
// Created by blues on 2024/12/31.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Macros.h>
#include <core/crypto/md5/MD5.h>
#include <core/name/Name.h>
#include <core/archive/MemoryArchive.h>
#include <core/event/Event.h>
#include <shader/ShaderCompiler.h>

#include <unordered_map>

namespace sky {

    static constexpr uint32_t SHADER_CACHE_MAGIC = SKY_COMBINE_CH_U32('S', 'E', 'S', 'D'); // sky engine shader

    struct ShaderCacheHeader {
        uint32_t magic = 0;
        uint32_t version = 0;
    };

    struct ShaderCacheEntry {
        Name savedFileName;
        uint32_t offset = 0;
        uint32_t length = 0;
    };

    struct ShaderCacheKey {
        Name entry;
        ShaderVariantKey key;
    };
} // namespaces sky

namespace std {

    template <>
    struct hash<sky::ShaderCacheKey> {
        size_t operator()(const sky::ShaderCacheKey &cacheKey) const noexcept
        {
            uint32_t val = 0;
            sky::HashCombine32(val, cacheKey.entry.GetHandle());
            sky::HashCombine32(val, static_cast<uint32_t>(std::hash<sky::ShaderVariantKey>()(cacheKey.key)));
            return val;
        }
    };

    template <>
    struct equal_to<sky::ShaderCacheKey> {
        bool operator()(const sky::ShaderCacheKey &x, const sky::ShaderCacheKey &y) const noexcept
        {
            return (x.entry == y.entry) && (x.key == y.key);
        }
    };

} // namespace std


namespace sky {
    struct ShaderSourceEntry {
        MD5 sourceMD5;
        std::vector<ShaderOptionEntry> options;
        std::unordered_map<ShaderCacheKey, MD5> entries;
    };

    struct ShaderCacheMapping {
        uint32_t version = 0;
        std::unordered_map<Name, ShaderSourceEntry> entries;
        std::unordered_map<MD5, ShaderCacheEntry> cacheEntries;
    };

    class IShaderCacheEvent : public EventTraits {
    public:
        IShaderCacheEvent() = default;
        virtual ~IShaderCacheEvent() = default;

        using KeyType   = void;
        using MutexType = void;

        virtual void OnShaderCacheSaved(ShaderCompileTarget target, const MD5 &md5, const ShaderCacheEntry& entry) = 0;
    };

    class ShaderCacheManager : public Singleton<ShaderCacheManager>, public IShaderCacheEvent {
    public:
        ShaderCacheManager();
        ~ShaderCacheManager() override = default;

        const ShaderSourceEntry* FetchSource(const Name &name, ShaderCompileTarget target);
        const ShaderSourceEntry* SaveShader(const Name& shader, ShaderCompileTarget target, const MD5& sourceMD5, const ShaderVariantList& list);

        void SaveBinaryCache(const Name& shader, ShaderCompileTarget target, const Name& entry,
            const ShaderVariantKey &key, const ShaderBuildResult &result);
        const ShaderCacheEntry *FetchBinaryCache(const Name &name, ShaderCompileTarget target, const Name &entry, ShaderVariantKey key);

        void SaveMappingFile() const;
        void LoadMappingFile(ShaderCompileTarget target);
    private:
        void OnShaderCacheSaved(ShaderCompileTarget target, const MD5 &md5, const ShaderCacheEntry& entry) override;

        mutable std::recursive_mutex mutex;
        std::unordered_map<ShaderCompileTarget, ShaderCacheMapping> cacheMapping;

        EventBinder<IShaderCacheEvent> cacheEvent;
    };

} // namespace sky
