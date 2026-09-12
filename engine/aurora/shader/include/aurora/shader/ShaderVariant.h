//
// Aurora shader variant: unified key-value variant model, 128-bit bitmask key,
// and data-driven schema. Strong (macro) vs weak (specialization constant) is
// NOT a distinction here: it is resolved by the compiler from shader reflection.
//

#pragma once

#include <core/name/Name.h>
#include <aurora/rhi/VertexSemantic.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sky::aurora {

    struct ShaderCompileResult;
    struct ShaderSpecialization;
    struct ShaderVariantSchema;

    // ---- variant entry (name -> value), no strong/weak distinction ----
    struct ShaderVariantEntry {
        Name     key;
        uint32_t value = 0;
    };

    struct ShaderVariant {
        std::vector<ShaderVariantEntry> entries;

        // unified, order-insensitive hash; a component of the cache key
        uint32_t ContentHash() const;

        // collect spec-constant entries into a specialization (id -> value)
        void BuildSpecialization(const ShaderVariantSchema &schema,
                                 ShaderSpecialization &out) const;

        // human-readable "key=value, key=value" dump
        std::string ToString() const;
    };

    // a vertex switch depends on a set of vertex semantics (all must be present)
    struct VertexVariantDef {
        Name                          name;       // "HAS_VERTEX_COLOR"
        std::vector<VertexSemantic>   semantics;  // {COLOR}
    };

    // compute switch values from a semantic mask and append to `out`
    void BuildVertexVariant(const std::vector<VertexVariantDef> &defs,
                            const VertexSemanticMask &mask, ShaderVariant &out);

    // ---- global reserved pipeline bits ----
    // NOTE: the pipeline variant keys live in aurora/pipeline/GlobalVariantLayout
    // (defined by PipelinePass, mirroring GlobalRenderResources). The shader
    // module only reserves the region; per-shader keys start after it.

    // ---- data-driven schema ----
    struct ShaderVariantSchema {
        struct Source {
            Name     name;
            uint16_t bitOffset = 0;
            uint8_t  bitWidth  = 0;
        };
        struct Entry {
            Name     key;
            Name     source;
            uint16_t bitOffset = 0;   // bit offset relative to its source
            uint8_t  bitWidth  = 0;
            uint32_t defaultValue = 0;
            bool     isSpec = false;  // true = specialization constant (weak variant)
            uint32_t specId = 0;      // valid when isSpec
        };

        std::vector<Source> sources;
        std::vector<Entry>  entries;
        uint32_t totalBits = 0;       // per-shader region size (caller adds pipeline reserved bits)

        bool Validate(std::string *error = nullptr) const;
        const Entry  *FindEntry(Name key) const;
        const Source *FindSource(Name name) const;
        uint16_t      EntryAbsoluteOffset(const Entry &entry) const;
    };

    // ---- 128-bit bitmask key ----
    class ShaderVariantKey {
    public:
        static constexpr uint32_t kMaxBits = 128;

        uint64_t words[2]  = {0, 0};
        uint32_t totalBits = 0;

        void Set(const ShaderVariantSchema &schema, Name key, uint32_t value);
        uint32_t Get(const ShaderVariantSchema &schema, Name key) const;
        void SetPipelineBit(uint16_t bitOffset, bool on);
        uint32_t GetPipelineBit(uint16_t bitOffset) const;

        // 16-bit vertex semantic region at `offset`
        void SetVertexSemantics(uint16_t offset, const VertexSemanticMask &mask);
        VertexSemanticMask GetVertexSemantics(uint16_t offset) const;

        // human-readable "key=value, key=value" dump of the per-shader keys
        std::string ToString(const ShaderVariantSchema &schema) const;

        // same, but reading keys at `baseOffset` (for composed keys where the
        // per-shader region starts after the reserved pipeline bits)
        std::string ToString(const ShaderVariantSchema &schema, uint16_t baseOffset) const;

        ShaderVariantKey &operator|=(const ShaderVariantKey &other);
        ShaderVariantKey &operator<<=(uint32_t bits);

        bool operator==(const ShaderVariantKey &other) const
        {
            return words[0] == other.words[0] && words[1] == other.words[1] &&
                   totalBits == other.totalBits;
        }

    private:
        uint32_t GetBits(uint16_t offset, uint8_t width) const;
        void     SetBits(uint16_t offset, uint8_t width, uint32_t value);
    };

    // ---- unified cache key + reserved cache interface ----
    struct ShaderCacheKey {
        uint64_t sourceHash  = 0;
        uint64_t variantHash = 0;
        uint32_t target      = 0;   // static_cast<uint32_t>(ShaderTarget)
    };

    class ShaderCache {
    public:
        virtual ~ShaderCache() = default;
        virtual bool Load(const ShaderCacheKey &key, ShaderCompileResult &out) = 0;
        virtual void Store(const ShaderCacheKey &key, const ShaderCompileResult &result) = 0;
    };

} // namespace sky::aurora
