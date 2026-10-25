//
// Typed logical selection is independent of the legacy 128-bit packed key.
// Shader declarations and target support determine macro/native lowering.
//

#pragma once

#include <aurora/rhi/ShaderArtifact.h>
#include <aurora/rhi/VertexSemantic.h>
#include <core/name/Name.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sky::aurora {

    struct ShaderCompileResult;
    struct ShaderSpecialization;
    struct ShaderVariantSchema;

    // ---- variant entry (name -> value), no strong/weak distinction ----
    struct ShaderVariantEntry {
        Name              key;
        uint32_t          value                                        = 0;
        ShaderVariantType type                                         = ShaderVariantType::UINT32;
        bool              operator==(const ShaderVariantEntry &) const = default;
    };

    struct ShaderVariant {
        std::vector<ShaderVariantEntry> entries;

        // unified, order-insensitive hash; a component of the cache key
        uint32_t ContentHash() const;

        // human-readable "key=value, key=value" dump
        std::string ToString() const;
    };

    // a vertex switch depends on a set of vertex semantics (all must be present)
    struct VertexVariantDef {
        Name                        name;      // "HAS_VERTEX_COLOR"
        std::vector<VertexSemantic> semantics; // {COLOR}
    };

    // compute switch values from a semantic mask and append to `out`
    void BuildVertexVariant(const std::vector<VertexVariantDef> &defs, const VertexSemanticMask &mask, ShaderVariant &out);

    // ---- global reserved pipeline bits ----
    // GlobalVariantLayout owns pipeline keys; per-shader keys follow its reserved region.

    // ---- data-driven schema ----
    enum class ShaderVariantDomain : uint8_t { SHADER, VERTEX, PIPELINE };
    struct ShaderVariantSchema {
        static constexpr uint32_t kMaxLogicalBits = 32768;
        static constexpr uint32_t kMaxEntries     = 1024;
        struct Source {
            Name     name;
            uint16_t bitOffset = 0;
            uint16_t bitWidth  = 0;
        };
        struct Entry {
            Name                key;
            Name                source;
            uint16_t            bitOffset       = 0; // bit offset relative to its source
            uint8_t             bitWidth        = 0;
            uint32_t            defaultValue    = 0;
            ShaderVariantDomain domain          = ShaderVariantDomain::SHADER;
            uint16_t            vertexSemantics = 0;
            ShaderVariantType   type            = ShaderVariantType::UINT32;
            Name                symbol;
        };

        std::vector<Source> sources;
        std::vector<Entry>  entries;
        uint32_t            totalBits            = 0; // per-shader region size (caller adds pipeline reserved bits)
        uint32_t            pipelineReservedBits = 0;

        bool          Validate(std::string *error = nullptr) const;
        const Entry  *FindEntry(Name key) const;
        const Source *FindSource(Name name) const;
        uint16_t      EntryAbsoluteOffset(const Entry &entry) const;
    };

    struct ShaderVariantSelection {
        ShaderVariant                   values;
        std::vector<ShaderVariantValue> identity;
        uint64_t                        schemaFingerprint = 0;
        uint64_t                        fingerprint       = 0;
    };

    bool ResolveShaderVariant(const ShaderVariantSchema &schema,
                              VertexSemanticMask         vertexFeatures,
                              const ShaderVariant       *pipelineValues,
                              const ShaderVariant       *shaderValues,
                              ShaderVariantSelection    &out,
                              std::string               &error);
    bool ValidateShaderVariantValue(const ShaderVariantSchema::Entry &declaration, const ShaderVariantEntry &value);

    bool CanonicalShaderVariants(const ShaderVariant                     *variant,
                                 const ShaderVariantSchema               &schema,
                                 ShaderTarget                             target,
                                 std::vector<ShaderVariantValue>         &out,
                                 std::string                             *error    = nullptr,
                                 const std::vector<ShaderVariantBinding> *bindings = nullptr);
    bool BuildShaderSpecialization(const ShaderVariant *values, const ShaderStageArtifact &artifact, ShaderSpecialization &out, std::string &error);
    uint64_t HashShaderVariants(const std::vector<ShaderVariantValue> &variants);

    // Merge the external P-domain schema with a per-shader S-domain schema for
    // resolver-side validation/canonicalization. Idempotent when the schema was
    // already published as combined (products-only).
    ShaderVariantSchema MergeVariantSchemas(const ShaderVariantSchema &shader, const ShaderVariantSchema *pipeline);
    ShaderVariantSchema SelectVariantDomain(const ShaderVariantSchema &schema, bool pipeline);
    bool MergeStageVariantSchemas(const ShaderVariantSchema &first, const ShaderVariantSchema &second, ShaderVariantSchema &out, std::string &error);

    // ---- 128-bit bitmask key ----
    class ShaderVariantKey {
    public:
        static constexpr uint32_t kMaxBits = 128;

        uint64_t words[2]  = {0, 0};
        uint32_t totalBits = 0;

        bool                    Set(const ShaderVariantSchema &schema, Name key, uint32_t value);
        std::optional<uint32_t> Get(const ShaderVariantSchema &schema, Name key) const;
        void                    SetPipelineBit(uint16_t bitOffset, bool on);
        uint32_t                GetPipelineBit(uint16_t bitOffset) const;

        // 16-bit vertex semantic region at `offset`
        void               SetVertexSemantics(uint16_t offset, const VertexSemanticMask &mask);
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
            return words[0] == other.words[0] && words[1] == other.words[1] && totalBits == other.totalBits;
        }

    private:
        uint32_t GetBits(uint16_t offset, uint8_t width) const;
        void     SetBits(uint16_t offset, uint8_t width, uint32_t value);
    };

    // ---- unified cache key + reserved cache interface ----
    struct ShaderCacheKey {
        uint64_t                        sourceHash  = 0;
        uint64_t                        variantHash = 0;
        uint64_t                        layoutFp    = 0; // GlobalVariantLayout fingerprint
        uint64_t                        schemaFp    = 0; // per-shader schema fingerprint
        uint64_t                        toolchainFp = 0; // compiler/toolchain fingerprint
        uint64_t                        entryHash   = 0; // hash of the entry point name (entry-level v1)
        uint32_t                        target      = 0; // static_cast<uint32_t>(ShaderTarget)
        uint32_t                        stage       = 0; // static_cast<uint32_t>(ShaderStageFlagBit)
        std::string                     sourcePath;
        std::string                     sourceEntry;
        std::vector<ShaderVariantValue> variants;
        ShaderTargetEnvironment         environment;
        ShaderProducerIdentity          producer;

        bool operator==(const ShaderCacheKey &) const = default;
    };

    class ShaderCache {
    public:
        virtual ~ShaderCache()                                                           = default;
        virtual bool Load(const ShaderCacheKey &key, ShaderCompileResult &out)           = 0;
        virtual bool Store(const ShaderCacheKey &key, const ShaderCompileResult &result) = 0;
    };

} // namespace sky::aurora
