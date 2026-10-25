#include <aurora/pipeline/ShaderVariantAssembler.h>

#include <algorithm>
#include <set>

namespace sky::aurora {
    bool BuildShaderVariant(const ShaderVariantInputs &inputs, ShaderVariantInfo &out, std::string *error)
    {
        const auto fail = [&](const std::string &message) {
            if (error)
                *error = message;
            return false;
        };
        if (!inputs.global || !inputs.schema)
            return fail("Variant assembler requires a pipeline layout and shader schema");
        const auto &global = *inputs.global;
        auto        schema = *inputs.schema;
        if (!global.schema.Validate(error) || !schema.Validate(error))
            return false;
        const uint32_t total = global.reservedBits + kVertexSemanticBits + schema.totalBits;
        for (const auto &entry : global.schema.entries)
            if (schema.FindEntry(entry.key))
                return fail("Variant key is declared by both pipeline and shader schema: " + std::string(entry.key.GetStr()));
        std::set<Name> vertexKeys;
        if (inputs.vertexDefs)
            for (const auto &def : *inputs.vertexDefs) {
                const auto entry = std::ranges::find(schema.entries, def.name, &ShaderVariantSchema::Entry::key);
                if (entry == schema.entries.end() || def.semantics.empty() || !vertexKeys.insert(def.name).second)
                    return fail("Invalid or duplicate vertex variant declaration");
                uint16_t semantics = 0;
                for (const auto semantic : def.semantics)
                    semantics |= uint16_t(1u << static_cast<uint8_t>(semantic));
                if (entry->domain == ShaderVariantDomain::VERTEX && entry->vertexSemantics != semantics)
                    return fail("Vertex variant definitions disagree");
                entry->domain          = ShaderVariantDomain::VERTEX;
                entry->vertexSemantics = semantics;
            }
        const auto             combined = MergeVariantSchemas(schema, &global.schema);
        ShaderVariantSelection selection;
        std::string            validation;
        if (!ResolveShaderVariant(combined, inputs.vertexMask, inputs.pipelineValues, inputs.shaderValues, selection, validation))
            return fail(validation);
        ShaderVariantInfo candidate;
        candidate.values      = selection.values;
        candidate.selection   = std::move(selection);
        candidate.variantHash = candidate.selection.fingerprint;
        if (total > ShaderVariantKey::kMaxBits) {
            out = std::move(candidate);
            return true;
        }
        candidate.hasPackedKey = true;
        ShaderVariantKey local;
        for (const auto &value : candidate.values.entries) {
            const bool packed = global.schema.FindEntry(value.key) ? candidate.key.Set(global.schema, value.key, value.value)
                                                                   : local.Set(schema, value.key, value.value);
            if (!packed)
                return fail("Selected value cannot be represented by the packed variant key");
        }
        local <<= global.ShaderRegionBase();
        candidate.key |= local;
        candidate.key.SetVertexSemantics(global.VertexRegionBase(), inputs.vertexMask);
        candidate.key.totalBits = total;
        out                     = std::move(candidate);
        return true;
    }
} // namespace sky::aurora
