#include <aurora/shader/ShaderTechnique.h>

#include <aurora/shader/ShaderResolver.h>

#include <algorithm>
#include <array>
#include <optional>

namespace sky::aurora {
    namespace {
        RendererError Invalid(std::string message)
        {
            return {.code = RendererErrorCode::INVALID_ARGUMENT, .message = std::move(message)};
        }
        bool Present(const ShaderRef &ref)
        {
            return !ref.relativePath.empty() || !ref.entry.empty();
        }
        std::optional<ShaderTarget> Target(API backend)
        {
            switch (backend) {
            case API::VULKAN: return ShaderTarget::SPIRV;
            case API::DX12: return ShaderTarget::DXIL;
            case API::METAL: return ShaderTarget::MSL;
            default: return {};
            }
        }
        struct Stage {
            ShaderStageFlagBit      kind;
            const ShaderRef        &source;
            ShaderStageArtifactPtr &artifact;
            ShaderVariantSchema     schema;
        };
    } // namespace

    std::variant<RendererError, RenderTechniqueData> CompileShaderTechnique(
        ShaderFileSystem &files, API backend, const ShaderTechnique &technique, IFileSystem *products, const ShaderTechniqueVariant *resolved)
    {
        const auto target = Target(backend);
        if (!target)
            return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Technique backend is unsupported"};
        const bool compute = Present(technique.compute);
        if (compute ? Present(technique.vertex) || Present(technique.fragment) : !Present(technique.vertex) || !Present(technique.fragment))
            return Invalid("Technique requires vertex + fragment or compute exclusively");
        RenderTechniqueData result;
        result.backend = backend;
        ShaderResolver      resolver(products, nullptr, files, 0, 0, products ? ShaderResolveMode::PRODUCTS : ShaderResolveMode::DEVELOPMENT);
        Stage               stages[] = {{ShaderStageFlagBit::VS, technique.vertex, result.vertex},
                                        {ShaderStageFlagBit::FS, technique.fragment, result.fragment},
                                        {ShaderStageFlagBit::CS, technique.compute, result.compute}};
        ShaderVariantSchema programSchema;
        std::string         error;
        const auto          pipelineSchema = resolved ? SelectVariantDomain(resolved->schema, true) : ShaderVariantSchema{};
        for (auto &stage : stages) {
            if (!Present(stage.source))
                continue;
            if (stage.source.relativePath.empty() || stage.source.entry.empty())
                return Invalid("Technique requires explicit stage paths and entry names");
            uint64_t fingerprint = 0;
            if (!resolver.ResolveSchema(stage.source.relativePath, static_cast<uint32_t>(*target), stage.schema, fingerprint, &error))
                return Invalid(error);
            if (resolved)
                stage.schema = MergeVariantSchemas(stage.schema, &pipelineSchema);
            ShaderVariantSchema merged;
            if (!MergeStageVariantSchemas(programSchema, stage.schema, merged, error))
                return Invalid(error);
            programSchema = std::move(merged);
        }
        ShaderVariantSelection selection;
        if (resolved) {
            ShaderVariant pipeline, local;
            for (const auto &value : resolved->macros.entries) {
                const auto *entry = programSchema.FindEntry(value.key);
                if (!entry)
                    return Invalid("Technique selection contains an undeclared key: " + std::string(value.key.GetStr()));
                if (entry->domain == ShaderVariantDomain::PIPELINE)
                    pipeline.entries.push_back(value);
                else if (entry->domain == ShaderVariantDomain::SHADER)
                    local.entries.push_back(value);
            }
            if (!ResolveShaderVariant(programSchema, resolved->vertexFeatures, &pipeline, &local, selection, error))
                return Invalid(error);
            std::vector<ShaderVariantValue> supplied;
            if (!CanonicalShaderVariants(&resolved->macros, programSchema, *target, supplied, &error) || supplied != selection.identity)
                return Invalid("Technique selection disagrees with its vertex features or normalized values: " + error);
        } else if (!ResolveShaderVariant(programSchema, {}, nullptr, &technique.variant, selection, error)) {
            return Invalid(error);
        }
        result.variantHash = selection.fingerprint;
        for (const auto &stage : stages)
            if (Present(stage.source)) {
                result.variantHash = HashMixU64(result.variantHash, HashString(stage.source.relativePath));
                result.variantHash = HashMixU64(result.variantHash, HashString(stage.source.entry));
                result.variantHash = HashMixU64(result.variantHash, static_cast<uint32_t>(stage.kind));
            }
        result.selection     = selection.identity;
        result.variantSchema = programSchema;
        for (auto &stage : stages) {
            if (!Present(stage.source))
                continue;
            ShaderVariant values;
            for (const auto &value : selection.values.entries)
                if (stage.schema.FindEntry(value.key))
                    values.entries.push_back(value);
            ShaderResolver::Request request;
            request.relativePath = stage.source.relativePath;
            request.entry        = stage.source.entry;
            request.stage        = stage.kind;
            request.target       = static_cast<uint32_t>(*target);
            request.variant      = &values;
            request.schema       = &stage.schema;
            request.variantHash  = selection.fingerprint;
            request.variantDump  = values.ToString();
            ShaderCompileResult compiled;
            if (!resolver.ResolveShader(request, compiled, &error))
                return Invalid("Technique compile failed: " + error);
            if (!compiled.reflection.pushConstants.empty())
                return Invalid("Technique push constants are not supported");
            if (!MergeShaderReflections(compiled.reflection, result.reflection, error) ||
                !BuildShaderSpecialization(&values, compiled, result.specialization, error))
                return Invalid(error);
            stage.artifact = std::make_shared<const ShaderStageArtifact>(std::move(compiled));
        }
        return result;
    }
} // namespace sky::aurora
