//
// Created on 2026/10/05.
//

#include <framework/asset/DerivedDataCache.h>
#include <core/logger/Logger.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace sky {

    namespace {

        const char *TAG = "DDC";
        constexpr uint64_t kFnvOffset = 1469598103934665603ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        void HashBytes(uint64_t &hash, const void *data, size_t size)
        {
            const auto *bytes = static_cast<const uint8_t *>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= bytes[i];
                hash *= kFnvPrime;
            }
        }

        std::string ToHex(uint64_t value)
        {
            char buffer[17] = {0};
            std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
            return buffer;
        }

    } // namespace

    DerivedDataCache &DerivedDataCache::Get()
    {
        static DerivedDataCache cache;
        return cache;
    }

    void DerivedDataCache::SetRoot(const std::string &dir)
    {
        if (!dir.empty()) {
            root = dir;
        }
    }

    void DerivedDataCache::Register(const std::shared_ptr<IDerivedDataBuilder> &builder)
    {
        if (builder) {
            builders[builder->GetId()] = builder;
        }
    }

    IDerivedDataBuilder *DerivedDataCache::Find(const std::string &builderId) const
    {
        const auto iter = builders.find(builderId);
        return iter == builders.end() ? nullptr : iter->second.get();
    }

    bool DerivedDataCache::Fetch(const std::vector<uint8_t> &source, const std::string &builderId,
                                 const std::string &settings, const std::string &platform,
                                 std::vector<uint8_t> &out)
    {
        IDerivedDataBuilder *builder = Find(builderId);
        const uint32_t version = builder != nullptr ? builder->GetVersion() : 0;

        uint64_t hash = kFnvOffset;
        HashBytes(hash, source.data(), source.size());
        HashBytes(hash, builderId.data(), builderId.size());
        HashBytes(hash, &version, sizeof(version));
        HashBytes(hash, settings.data(), settings.size());
        HashBytes(hash, platform.data(), platform.size());
        const std::string key = ToHex(hash);

        std::error_code error;
        const std::filesystem::path path = std::filesystem::path(root) / (key + ".bin");
        if (std::filesystem::exists(path, error)) {
            std::ifstream in(path, std::ios::binary);
            if (in) {
                out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                return !out.empty();
            }
        }

        if (builder == nullptr) {
            LOG_E(TAG, "no builder registered: %s", builderId.c_str());
            return false;
        }
        if (!builder->Build(source, settings, out)) {
            return false;
        }

        std::filesystem::create_directories(root, error);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (file) {
            file.write(reinterpret_cast<const char *>(out.data()), static_cast<std::streamsize>(out.size()));
        }
        return !out.empty();
    }

} // namespace sky
