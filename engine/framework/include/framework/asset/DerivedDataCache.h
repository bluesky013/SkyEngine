//
// Created on 2026/10/05.
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky {

    // A processor that turns source bytes into derived (cacheable) bytes.
    // Builders are registered by id and implemented by feature modules; they only
    // depend on their own core module and any third-party they need.
    class IDerivedDataBuilder {
    public:
        virtual ~IDerivedDataBuilder() = default;

        virtual std::string GetId() const = 0;
        virtual uint32_t GetVersion() const { return 1; }
        virtual bool Build(const std::vector<uint8_t> &source, const std::string &settings,
                           std::vector<uint8_t> &out) const = 0;
    };

    // Content-addressed derived-data cache (DDC). The cache key is
    // hash(source + builder id + builder version + settings + platform); on a miss
    // the registered builder runs, its output is written, and returned; later
    // fetches read the cache. Any input change invalidates the entry.
    class DerivedDataCache {
    public:
        static DerivedDataCache &Get();

        // Directory used for cached entries (created on first write).
        void SetRoot(const std::string &dir);
        const std::string &GetRoot() const { return root; }

        void Register(const std::shared_ptr<IDerivedDataBuilder> &builder);
        IDerivedDataBuilder *Find(const std::string &builderId) const;

        bool Fetch(const std::vector<uint8_t> &source, const std::string &builderId,
                   const std::string &settings, const std::string &platform, std::vector<uint8_t> &out);

    private:
        DerivedDataCache() = default;

        std::string root = "ddc-cache";
        std::unordered_map<std::string, std::shared_ptr<IDerivedDataBuilder>> builders;
    };

} // namespace sky
