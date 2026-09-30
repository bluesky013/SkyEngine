//
// ShaderVariantAssembler implementation.
//

#include <aurora/pipeline/ShaderVariantAssembler.h>

#include <aurora/shader/ShaderHash.h>

namespace sky::aurora {

    namespace {
        const ShaderVariantEntry *FindEntry(const ShaderVariant *variant, Name key)
        {
            if (variant == nullptr) {
                return nullptr;
            }
            for (const auto &e : variant->entries) {
                if (e.key == key) {
                    return &e;
                }
            }
            return nullptr;
        }
    } // namespace

    bool BuildShaderVariant(const ShaderVariantInputs &inputs, ShaderVariantInfo &out, std::string *error)
    {
        if (inputs.global == nullptr || inputs.schema == nullptr) {
            if (error != nullptr) {
                *error = "variant assembler requires global layout and schema";
            }
            return false;
        }

        const GlobalVariantLayout &global = *inputs.global;
        const ShaderVariantSchema &schema = *inputs.schema;

        const uint32_t total = static_cast<uint32_t>(global.reservedBits) + kVertexSemanticBits + schema.totalBits;
        if (total > ShaderVariantKey::kMaxBits) {
            if (error != nullptr) {
                *error = "pipeline + vertex + shader variant bits exceed 128";
            }
            return false;
        }

        out.key = ShaderVariantKey{};
        out.spec.entries.clear();

        // ---- pipeline region (P) ----
        for (const auto &entry : global.schema.entries) {
            uint32_t value = entry.defaultValue;
            if (const auto *o = FindEntry(inputs.overrides, entry.key)) {
                value = o->value;
            }
            out.key.Set(global.schema, entry.key, value);
        }

        // ---- vertex semantics region (V) ----
        out.key.SetVertexSemantics(global.VertexRegionBase(), inputs.vertexMask);

        // vertex switches (name -> value derived from the mask)
        ShaderVariant vertexVariants;
        if (inputs.vertexDefs != nullptr) {
            BuildVertexVariant(*inputs.vertexDefs, inputs.vertexMask, vertexVariants);
        }

        // ---- per-shader schema region (S) ----
        ShaderVariantKey perKey;
        for (const auto &entry : schema.entries) {
            uint32_t value = entry.defaultValue;
            if (const auto *vv = FindEntry(&vertexVariants, entry.key)) {
                value = vv->value;
            }
            if (const auto *o = FindEntry(inputs.overrides, entry.key)) {
                value = o->value; // material override wins over vertex switch / default
            }

            if (entry.isSpec) {
                out.spec.entries.push_back({entry.specId, value});
            } else {
                perKey.Set(schema, entry.key, value);
            }
        }
        perKey <<= global.ShaderRegionBase();
        out.key |= perKey;
        out.key.totalBits = total;

        // ---- hash (v1: spec values folded in) ----
        uint64_t hash = kFnv1aBasis;
        hash          = HashMixU64(hash, out.key.words[0]);
        hash          = HashMixU64(hash, out.key.words[1]);
        hash          = HashMixU64(hash, out.key.totalBits);
        for (const auto &s : out.spec.entries) {
            hash = HashMixU64(hash, s.id);
            hash = HashMixU64(hash, s.value);
        }
        out.variantHash = hash;
        return true;
    }

} // namespace sky::aurora
