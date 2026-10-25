//
// ShaderVariant implementation: bitmask key, schema validation, and hash.
//

#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderHash.h>
#include <aurora/shader/ShaderVariant.h>
#include <core/hash/Fnv1a.h>

#include <algorithm>
#include <bit>
#include <set>

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
        std::sort(sorted.begin(), sorted.end(), [](const ShaderVariantEntry &a, const ShaderVariantEntry &b) {
            return std::string_view(a.key.GetStr()) < std::string_view(b.key.GetStr());
        });

        uint32_t hash = FNVOffsetBias32;
        for (const auto &e : sorted) {
            const std::string_view name(e.key.GetStr());
            hash = Fnv1aCombine(hash, static_cast<uint32_t>(name.size()));
            for (const char c : name) {
                hash ^= static_cast<uint8_t>(c);
                hash *= FNVPrime32;
            }
            hash = Fnv1aCombine(hash, e.value);
            hash = Fnv1aCombine(hash, static_cast<uint32_t>(e.type));
        }
        return hash;
    }

    uint64_t HashShaderVariants(const std::vector<ShaderVariantValue> &variants)
    {
        auto hash = HashMixU64(kFnv1aBasis, variants.size());
        for (const auto &entry : variants) {
            hash = HashMixBytes(HashMixU64(hash, entry.name.size()), entry.name);
            hash = HashMixU64(hash, entry.value);
            hash = HashMixU64(hash, static_cast<uint32_t>(entry.type));
        }
        return hash;
    }

    bool ValidateShaderVariantValue(const ShaderVariantSchema::Entry &declaration, const ShaderVariantEntry &value)
    {
        if (value.type != declaration.type || !declaration.bitWidth || declaration.bitWidth > 32)
            return false;
        switch (value.type) {
        case ShaderVariantType::BOOL: return declaration.bitWidth == 1 && value.value <= 1;
        case ShaderVariantType::INT32: {
            const int64_t signedValue = std::bit_cast<int32_t>(value.value);
            const int64_t limit       = int64_t{1} << (declaration.bitWidth - 1);
            return signedValue >= -limit && signedValue < limit;
        }
        case ShaderVariantType::UINT32: return declaration.bitWidth == 32 || value.value < (uint32_t{1} << declaration.bitWidth);
        default: return false;
        }
    }

    bool ResolveShaderVariant(const ShaderVariantSchema &schema,
                              VertexSemanticMask         vertexFeatures,
                              const ShaderVariant       *pipelineValues,
                              const ShaderVariant       *shaderValues,
                              ShaderVariantSelection    &out,
                              std::string               &error)
    {
        error.clear();
        if (!schema.Validate(&error))
            return false;
        const auto validate = [&](const ShaderVariant *values, ShaderVariantDomain owner) {
            std::set<Name> seen;
            if (!values)
                return true;
            for (const auto &value : values->entries) {
                const auto *entry = schema.FindEntry(value.key);
                if (!seen.insert(value.key).second || !entry || entry->domain != owner || !ValidateShaderVariantValue(*entry, value)) {
                    error = "Duplicate, unknown, wrong-domain or invalid typed variant: " + std::string(value.key.GetStr());
                    return false;
                }
            }
            return true;
        };
        if (!validate(pipelineValues, ShaderVariantDomain::PIPELINE) || !validate(shaderValues, ShaderVariantDomain::SHADER))
            return false;
        ShaderVariantSelection candidate;
        for (const auto &entry : schema.entries) {
            auto value = entry.defaultValue;
            if (entry.domain == ShaderVariantDomain::VERTEX) {
                value = (vertexFeatures.mask & entry.vertexSemantics) == entry.vertexSemantics ? 1u : 0u;
            } else {
                const auto *values = entry.domain == ShaderVariantDomain::PIPELINE ? pipelineValues : shaderValues;
                if (values)
                    for (const auto &input : values->entries)
                        if (input.key == entry.key)
                            value = input.value;
            }
            candidate.identity.push_back({std::string(entry.key.GetStr()), value, entry.type});
        }
        std::sort(candidate.identity.begin(), candidate.identity.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
        for (const auto &value : candidate.identity)
            candidate.values.entries.push_back({Name(value.name.c_str()), value.value, value.type});
        candidate.schemaFingerprint = HashSchema(schema);
        candidate.fingerprint       = HashMixU64(candidate.schemaFingerprint, HashShaderVariants(candidate.identity));
        out                         = std::move(candidate);
        return true;
    }

    ShaderVariantSchema MergeVariantSchemas(const ShaderVariantSchema &shader, const ShaderVariantSchema *pipeline)
    {
        if (!pipeline)
            return shader;
        ShaderVariantSchema merged;
        merged.pipelineReservedBits = pipeline->pipelineReservedBits;
        const auto append           = [&](ShaderVariantSchema::Entry entry) {
            entry.source    = Name("combined");
            entry.bitOffset = static_cast<uint16_t>(merged.totalBits);
            merged.totalBits += entry.bitWidth;
            merged.entries.push_back(entry);
        };
        for (auto entry : pipeline->entries) {
            entry.domain = ShaderVariantDomain::PIPELINE;
            append(entry);
        }
        for (const auto &entry : shader.entries) {
            if (auto existing = std::ranges::find(merged.entries, entry.key, &ShaderVariantSchema::Entry::key); existing != merged.entries.end()) {
                if (entry.domain != ShaderVariantDomain::PIPELINE || entry.bitWidth != existing->bitWidth ||
                    entry.defaultValue != existing->defaultValue || entry.type != existing->type) {
                    merged.totalBits = UINT32_MAX;
                    return merged;
                }
                existing->symbol = entry.symbol;
            } else {
                append(entry);
            }
        }
        if (merged.totalBits)
            merged.sources.push_back({Name("combined"), 0, static_cast<uint16_t>(merged.totalBits)});
        return merged;
    }

    ShaderVariantSchema SelectVariantDomain(const ShaderVariantSchema &schema, bool pipeline)
    {
        ShaderVariantSchema result;
        result.pipelineReservedBits = pipeline ? schema.pipelineReservedBits : 0;
        for (const auto &entry : schema.entries) {
            if ((entry.domain == ShaderVariantDomain::PIPELINE) != pipeline)
                continue;
            auto copy      = entry;
            copy.source    = Name("selection");
            copy.bitOffset = static_cast<uint16_t>(result.totalBits);
            result.totalBits += copy.bitWidth;
            result.entries.push_back(copy);
        }
        if (result.totalBits)
            result.sources.push_back({Name("selection"), 0, static_cast<uint16_t>(result.totalBits)});
        return result;
    }

    bool MergeStageVariantSchemas(const ShaderVariantSchema &first, const ShaderVariantSchema &second, ShaderVariantSchema &out, std::string &error)
    {
        auto result = first;
        for (const auto &entry : second.entries) {
            if (const auto *existing = result.FindEntry(entry.key)) {
                if (existing->bitWidth != entry.bitWidth || existing->defaultValue != entry.defaultValue || existing->type != entry.type ||
                    existing->domain != entry.domain || existing->vertexSemantics != entry.vertexSemantics) {
                    error = "Technique stages disagree on variant declaration: " + std::string(entry.key.GetStr());
                    return false;
                }
            } else {
                result.entries.push_back(entry);
            }
        }
        result.totalBits = 0;
        for (auto &entry : result.entries) {
            entry.source    = Name("selection");
            entry.bitOffset = static_cast<uint16_t>(result.totalBits);
            result.totalBits += entry.bitWidth;
        }
        result.sources.clear();
        if (result.totalBits)
            result.sources.push_back({Name("selection"), 0, static_cast<uint16_t>(result.totalBits)});
        if (!result.Validate(&error))
            return false;
        out = std::move(result);
        return true;
    }

    bool CanonicalShaderVariants(const ShaderVariant                     *variant,
                                 const ShaderVariantSchema               &schema,
                                 ShaderTarget                             target,
                                 std::vector<ShaderVariantValue>         &out,
                                 std::string                             *error,
                                 const std::vector<ShaderVariantBinding> *bindings)
    {
        if (!schema.Validate(error)) {
            return false;
        }
        for (size_t i = 0; i < schema.entries.size(); ++i) {
            const auto &entry = schema.entries[i];
            if (entry.key.GetStr().empty() || !ValidateShaderVariantValue(entry, {entry.key, entry.defaultValue, entry.type})) {
                if (error != nullptr) {
                    *error = "invalid shader variant declaration";
                }
                return false;
            }
            for (size_t j = 0; j < i; ++j) {
                if (schema.entries[j].key == entry.key) {
                    if (error != nullptr) {
                        *error = "duplicate shader variant key or specialization ID";
                    }
                    return false;
                }
            }
        }
        if (variant != nullptr) {
            for (size_t i = 0; i < variant->entries.size(); ++i) {
                const auto &entry      = variant->entries[i];
                const auto *definition = schema.FindEntry(entry.key);
                if (definition == nullptr || !ValidateShaderVariantValue(*definition, entry)) {
                    if (error != nullptr) {
                        *error = "unknown or out-of-range shader variant: " + std::string(entry.key.GetStr());
                    }
                    return false;
                }
                for (size_t j = 0; j < i; ++j) {
                    if (variant->entries[j].key == entry.key) {
                        if (error != nullptr) {
                            *error = "duplicate shader variant: " + std::string(entry.key.GetStr());
                        }
                        return false;
                    }
                }
            }
        }
        std::vector<ShaderVariantValue> values;
        for (const auto &entry : schema.entries) {
            if (bindings && std::ranges::any_of(*bindings, [&](const auto &binding) {
                    return binding.key == entry.key.GetStr() && binding.lowering == ShaderVariantLowering::NATIVE;
                }))
                continue;
            auto value = entry.defaultValue;
            if (variant != nullptr) {
                for (const auto &input : variant->entries) {
                    if (input.key == entry.key) {
                        value = input.value;
                        break;
                    }
                }
            }
            values.push_back({std::string(entry.key.GetStr()), value, entry.type});
        }
        std::sort(values.begin(), values.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
        out = std::move(values);
        return true;
    }

    bool BuildShaderSpecialization(const ShaderVariant *values, const ShaderStageArtifact &artifact, ShaderSpecialization &out, std::string &error)
    {
        auto candidate = out;
        for (const auto &binding : artifact.variantBindings) {
            if (binding.lowering != ShaderVariantLowering::NATIVE)
                continue;
            uint32_t bits = binding.defaultValue;
            if (values)
                for (const auto &value : values->entries)
                    if (binding.key == value.key.GetStr()) {
                        if (binding.type != value.type) {
                            error = "Native variant type mismatch: " + binding.key;
                            return false;
                        }
                        bits = value.value;
                    }
            const auto duplicate = std::ranges::find_if(
                candidate.entries, [&](const auto &value) { return value.stage == artifact.identity.stage && value.id == binding.nativeId; });
            if (duplicate != candidate.entries.end()) {
                error = "Duplicate stage specialization value: " + binding.key;
                return false;
            }
            candidate.entries.push_back({binding.nativeId, bits, artifact.identity.stage, binding.type, binding.key});
        }
        out = std::move(candidate);
        return true;
    }

    void BuildVertexVariant(const std::vector<VertexVariantDef> &defs, const VertexSemanticMask &mask, ShaderVariant &out)
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
            s += std::string(entry.key.GetStr()) + "=" +
                 (entry.type == ShaderVariantType::INT32 ? std::to_string(std::bit_cast<int32_t>(entry.value)) : std::to_string(entry.value));
        }
        return s;
    }

    std::string ShaderVariantKey::ToString(const ShaderVariantSchema &schema) const
    {
        return ToString(schema, 0);
    }

    std::string ShaderVariantKey::ToString(const ShaderVariantSchema &schema, uint16_t baseOffset) const
    {
        if (uint32_t(baseOffset) + schema.totalBits > kMaxBits || !schema.Validate())
            return "<unrepresentable packed variant>";
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
        if (totalBits > kMaxLogicalBits || entries.size() > kMaxEntries || pipelineReservedBits > ShaderVariantKey::kMaxBits) {
            if (error != nullptr) {
                *error = "variant schema exceeds the logical metadata budget";
            }
            return false;
        }

        std::set<Name> sourceNames;
        for (const auto &source : sources) {
            if (source.name.Empty() || !sourceNames.insert(source.name).second || uint32_t(source.bitOffset) + source.bitWidth > totalBits) {
                if (error)
                    *error = "invalid or duplicate variant source";
                return false;
            }
        }
        if (sources.size() > kMaxEntries) {
            if (error)
                *error = "variant sources exceed the metadata budget";
            return false;
        }

        // absolute [offset, offset+width) intervals, for overlap detection
        std::vector<std::pair<uint16_t, uint16_t>> intervals;
        intervals.reserve(entries.size());
        std::set<Name> keys;

        for (const auto &entry : entries) {
            if (entry.key.Empty() || !keys.insert(entry.key).second ||
                !ValidateShaderVariantValue(entry, {entry.key, entry.defaultValue, entry.type})) {
                if (error)
                    *error = "invalid or duplicate variant declaration";
                return false;
            }
            if (entry.domain > ShaderVariantDomain::PIPELINE ||
                (entry.domain == ShaderVariantDomain::VERTEX ? !entry.vertexSemantics || entry.bitWidth != 1 : entry.vertexSemantics != 0)) {
                if (error)
                    *error = "invalid variant ownership metadata";
                return false;
            }
            const Source *source = FindSource(entry.source);
            if (source == nullptr) {
                if (error != nullptr) {
                    *error = "variant entry references unknown source";
                }
                return false;
            }
            const uint32_t abs = static_cast<uint32_t>(source->bitOffset) + entry.bitOffset;
            const uint32_t end = abs + entry.bitWidth;
            if (end > totalBits || uint32_t(entry.bitOffset) + entry.bitWidth > source->bitWidth) {
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
        return static_cast<uint16_t>((source != nullptr ? source->bitOffset : 0) + entry.bitOffset);
    }

    bool ShaderVariantKey::Set(const ShaderVariantSchema &schema, Name key, uint32_t value)
    {
        const auto *entry = schema.FindEntry(key);
        if (schema.totalBits > kMaxBits || !schema.Validate() || !entry || !ValidateShaderVariantValue(*entry, {key, value, entry->type}))
            return false;
        SetBits(schema.EntryAbsoluteOffset(*entry), entry->bitWidth, value);
        totalBits = schema.totalBits;
        return true;
    }

    std::optional<uint32_t> ShaderVariantKey::Get(const ShaderVariantSchema &schema, Name key) const
    {
        const auto *entry = schema.FindEntry(key);
        if (schema.totalBits > kMaxBits || !schema.Validate() || !entry)
            return {};
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
