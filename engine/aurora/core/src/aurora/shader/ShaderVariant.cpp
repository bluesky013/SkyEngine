//
// ShaderVariant implementation: bitmask key, schema validation, and hash.
//

#include <aurora/shader/ShaderVariant.h>
#include <aurora/rhi/Shader.h>
#include <core/hash/Fnv1a.h>

#include <algorithm>

namespace sky::aurora {

    namespace {
        uint32_t Fnv1aCombine(uint32_t hash, uint32_t data)
        {
            for (int i = 0; i < 4; ++i) {
                hash ^= (data >> (i * 8)) & 0xFFu;
                hash *= FNVPrime32;
            }
            return hash;
        }
    } // namespace

    uint32_t ShaderVariant::ContentHash() const
    {
        std::vector<ShaderVariantEntry> sorted = entries;
        std::sort(sorted.begin(), sorted.end(),
                  [](const ShaderVariantEntry &a, const ShaderVariantEntry &b) {
                      return a.key.GetHandle() < b.key.GetHandle();
                  });

        uint32_t hash = FNVOffsetBias32;
        for (const auto &e : sorted) {
            hash = Fnv1aCombine(hash, e.key.GetHandle());
            hash = Fnv1aCombine(hash, e.value);
        }
        return hash;
    }

    void ShaderVariant::BuildSpecialization(const ShaderVariantSchema &schema,
                                            ShaderSpecialization &out) const
    {
        out.entries.clear();
        for (const auto &entry : entries) {
            const ShaderVariantSchema::Entry *se = schema.FindEntry(entry.key);
            if (se != nullptr && se->isSpec) {
                out.entries.push_back({se->specId, entry.value});
            }
        }
    }

    void BuildVertexVariant(const std::vector<VertexVariantDef> &defs,
                            const VertexSemanticMask &mask, ShaderVariant &out)
    {
        for (const auto &def : defs) {
            bool on = true;
            for (const auto semantic : def.semantics) {
                if (!mask.Test(semantic)) {
                    on = false;
                    break;
                }
            }
            out.entries.push_back({def.name, on ? 1u : 0u});
        }
    }

    std::string ShaderVariant::ToString() const
    {
        std::string s;
        for (const auto &entry : entries) {
            if (!s.empty()) {
                s += ", ";
            }
            s += std::string(entry.key.GetStr()) + "=" + std::to_string(entry.value);
        }
        return s;
    }

    std::string ShaderVariantKey::ToString(const ShaderVariantSchema &schema) const
    {
        return ToString(schema, 0);
    }

    std::string ShaderVariantKey::ToString(const ShaderVariantSchema &schema,
                                           uint16_t baseOffset) const
    {
        std::string s;
        for (const auto &entry : schema.entries) {
            if (!s.empty()) {
                s += ", ";
            }
            const uint16_t abs = static_cast<uint16_t>(baseOffset + schema.EntryAbsoluteOffset(entry));
            s += std::string(entry.key.GetStr()) + "=" + std::to_string(GetBits(abs, entry.bitWidth));
        }
        return s;
    }

    bool ShaderVariantSchema::Validate(std::string *error) const
    {
        if (totalBits > ShaderVariantKey::kMaxBits) {
            if (error != nullptr) {
                *error = "variant schema exceeds 128 bits";
            }
            return false;
        }

        // absolute [offset, offset+width) intervals, for overlap detection
        std::vector<std::pair<uint16_t, uint16_t>> intervals;
        intervals.reserve(entries.size());

        for (const auto &entry : entries) {
            const Source *source = FindSource(entry.source);
            if (source == nullptr) {
                if (error != nullptr) {
                    *error = "variant entry references unknown source";
                }
                return false;
            }
            const uint32_t abs    = static_cast<uint32_t>(source->bitOffset) + entry.bitOffset;
            const uint32_t end    = abs + entry.bitWidth;
            if (end > totalBits) {
                if (error != nullptr) {
                    *error = "variant entry exceeds schema total bits";
                }
                return false;
            }
            intervals.emplace_back(static_cast<uint16_t>(abs), static_cast<uint16_t>(end));
        }

        std::sort(intervals.begin(), intervals.end());
        for (size_t i = 1; i < intervals.size(); ++i) {
            if (intervals[i].first < intervals[i - 1].second) {
                if (error != nullptr) {
                    *error = "variant entries overlap";
                }
                return false;
            }
        }
        return true;
    }

    const ShaderVariantSchema::Entry *ShaderVariantSchema::FindEntry(Name key) const
    {
        for (const auto &entry : entries) {
            if (entry.key == key) {
                return &entry;
            }
        }
        return nullptr;
    }

    const ShaderVariantSchema::Source *ShaderVariantSchema::FindSource(Name name) const
    {
        for (const auto &source : sources) {
            if (source.name == name) {
                return &source;
            }
        }
        return nullptr;
    }

    uint16_t ShaderVariantSchema::EntryAbsoluteOffset(const Entry &entry) const
    {
        const Source *source = FindSource(entry.source);
        return static_cast<uint16_t>(
            (source != nullptr ? source->bitOffset : 0) + entry.bitOffset);
    }

    void ShaderVariantKey::Set(const ShaderVariantSchema &schema, Name key, uint32_t value)
    {
        totalBits = schema.totalBits;
        const ShaderVariantSchema::Entry *entry = schema.FindEntry(key);
        if (entry == nullptr) {
            return;
        }
        SetBits(schema.EntryAbsoluteOffset(*entry), entry->bitWidth, value);
    }

    uint32_t ShaderVariantKey::Get(const ShaderVariantSchema &schema, Name key) const
    {
        const ShaderVariantSchema::Entry *entry = schema.FindEntry(key);
        if (entry == nullptr) {
            return 0;
        }
        return GetBits(schema.EntryAbsoluteOffset(*entry), entry->bitWidth);
    }

    void ShaderVariantKey::SetPipelineBit(uint16_t bitOffset, bool on)
    {
        const int word  = bitOffset / 64;
        const int shift = bitOffset % 64;
        if (on) {
            words[word] |= (1ull << shift);
        } else {
            words[word] &= ~(1ull << shift);
        }
    }

    uint32_t ShaderVariantKey::GetPipelineBit(uint16_t bitOffset) const
    {
        const int word  = bitOffset / 64;
        const int shift = bitOffset % 64;
        return (words[word] >> shift) & 1u;
    }

    void ShaderVariantKey::SetVertexSemantics(uint16_t offset, const VertexSemanticMask &mask)
    {
        for (uint16_t i = 0; i < kVertexSemanticBits; ++i) {
            const uint16_t bit   = static_cast<uint16_t>(offset + i);
            const int      word  = bit / 64;
            const int      shift = bit % 64;
            if (mask.mask & (1u << i)) {
                words[word] |= (1ull << shift);
            } else {
                words[word] &= ~(1ull << shift);
            }
        }
    }

    VertexSemanticMask ShaderVariantKey::GetVertexSemantics(uint16_t offset) const
    {
        VertexSemanticMask mask;
        for (uint16_t i = 0; i < kVertexSemanticBits; ++i) {
            const uint16_t bit   = static_cast<uint16_t>(offset + i);
            const int      word  = bit / 64;
            const int      shift = bit % 64;
            if (words[word] & (1ull << shift)) {
                mask.mask |= static_cast<uint16_t>(1u << i);
            }
        }
        return mask;
    }

    ShaderVariantKey &ShaderVariantKey::operator|=(const ShaderVariantKey &other)
    {
        words[0] |= other.words[0];
        words[1] |= other.words[1];
        totalBits = std::max(totalBits, other.totalBits);
        return *this;
    }

    ShaderVariantKey &ShaderVariantKey::operator<<=(uint32_t bits)
    {
        if (bits >= 128) {
            words[0] = 0;
            words[1] = 0;
        } else if (bits >= 64) {
            words[1] = words[0] << (bits - 64);
            words[0] = 0;
        } else if (bits > 0) {
            words[1] = (words[1] << bits) | (words[0] >> (64 - bits));
            words[0] <<= bits;
        }
        totalBits += bits;
        return *this;
    }

    uint32_t ShaderVariantKey::GetBits(uint16_t offset, uint8_t width) const
    {
        uint32_t value = 0;
        for (uint8_t i = 0; i < width; ++i) {
            const uint16_t bit   = static_cast<uint16_t>(offset + i);
            const int      word  = bit / 64;
            const int      shift = bit % 64;
            if (words[word] & (1ull << shift)) {
                value |= (1u << i);
            }
        }
        return value;
    }

    void ShaderVariantKey::SetBits(uint16_t offset, uint8_t width, uint32_t value)
    {
        const uint32_t mask = width >= 32 ? 0xFFFFFFFFu : ((1u << width) - 1u);
        value &= mask;
        for (uint8_t i = 0; i < width; ++i) {
            const uint16_t bit   = static_cast<uint16_t>(offset + i);
            const int      word  = bit / 64;
            const int      shift = bit % 64;
            if (value & (1u << i)) {
                words[word] |= (1ull << shift);
            } else {
                words[word] &= ~(1ull << shift);
            }
        }
    }

} // namespace sky::aurora
