//
// ShaderHash: shared FNV-1a helpers used across the shader cache / variant
// hashing so the constants and mixing live in one place.
//

#pragma once

#include <aurora/shader/ShaderVariant.h>

#include <cstdint>
#include <string_view>

namespace sky::aurora {

    constexpr uint64_t kFnv1aBasis = 1469598103934665603ull;
    constexpr uint64_t kFnv1aPrime = 1099511628211ull;

    inline uint64_t HashMixU64(uint64_t hash, uint64_t data)
    {
        for (int i = 0; i < 8; ++i) {
            hash ^= (data >> (i * 8)) & 0xFFu;
            hash *= kFnv1aPrime;
        }
        return hash;
    }

    inline uint64_t HashMixBytes(uint64_t hash, std::string_view bytes)
    {
        for (char c : bytes) {
            hash ^= static_cast<unsigned char>(c);
            hash *= kFnv1aPrime;
        }
        return hash;
    }

    inline uint64_t HashString(std::string_view text)
    {
        return HashMixBytes(kFnv1aBasis, text);
    }

    inline uint64_t HashEntryName(std::string_view entry)
    {
        return HashString(entry);
    }

    inline uint64_t HashSchema(const ShaderVariantSchema &schema)
    {
        uint64_t hash = kFnv1aBasis;
        for (const auto &s : schema.sources) {
            hash = HashMixU64(hash, s.name.GetHandle());
            hash = HashMixU64(hash, s.bitOffset);
            hash = HashMixU64(hash, s.bitWidth);
        }
        for (const auto &e : schema.entries) {
            hash = HashMixU64(hash, e.key.GetHandle());
            hash = HashMixU64(hash, e.source.GetHandle());
            hash = HashMixU64(hash, e.bitOffset);
            hash = HashMixU64(hash, e.bitWidth);
            hash = HashMixU64(hash, e.defaultValue);
            hash = HashMixU64(hash, e.isSpec ? 1u : 0u);
            hash = HashMixU64(hash, e.specId);
        }
        hash = HashMixU64(hash, schema.totalBits);
        return hash;
    }

} // namespace sky::aurora
