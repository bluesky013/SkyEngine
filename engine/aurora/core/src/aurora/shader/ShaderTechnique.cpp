#include <aurora/shader/ShaderTechnique.h>

#include <algorithm>
#include <aurora/shader/ShaderResolver.h>
#include <cstring>
#include <optional>
#include <set>

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
        std::optional<RendererError> Validate(const ShaderTechnique &technique)
        {
            const bool graphics = Present(technique.vertex) && Present(technique.fragment);
            if (Present(technique.compute) ? Present(technique.vertex) || Present(technique.fragment) : !graphics) {
                return Invalid("Technique requires vertex + fragment or compute exclusively");
            }
            std::set<Name> keys;
            for (const auto &value : technique.variant.entries) {
                if (value.key.Empty() || !keys.insert(value.key).second) {
                    return Invalid("Technique has an empty or duplicate variant key");
                }
            }
            return {};
        }
        std::optional<RendererError>
        MergeSpecialization(const ShaderVariant &variant, const ShaderVariantSchema &schema, ShaderSpecialization &merged)
        {
            for (const auto &value : variant.entries) {
                const auto *field = schema.FindEntry(value.key);
                if (field == nullptr || (field->bitWidth < 32 && value.value >= (1u << field->bitWidth))) {
                    return Invalid("Technique variant is not supported by its shader schema");
                }
            }
            ShaderSpecialization stage;
            variant.BuildSpecialization(schema, stage);
            for (const auto &value : stage.entries) {
                const auto found = std::ranges::find(merged.entries, value.id, &ShaderSpecializationEntry::id);
                if (found == merged.entries.end()) {
                    merged.entries.push_back(value);
                } else if (found->value != value.value) {
                    return Invalid("Technique stages disagree on specialization values");
                }
            }
            return {};
        }
        struct StageOutput {
            ShaderStageFlagBit      stage;
            const ShaderRef        &source;
            ShaderStageArtifactPtr &artifact;
        };
        std::optional<RendererError> CompileStage(
            ShaderResolver &resolver, ShaderTarget target, const ShaderVariant &variant, const StageOutput &output, RenderTechniqueData &result)
        {
            if (output.source.relativePath.empty() || output.source.entry.empty()) {
                return Invalid("Technique stages require explicit source paths and entry names");
            }
            ShaderResolver::Request request;
            request.relativePath = output.source.relativePath;
            request.entry        = output.source.entry;
            request.stage        = output.stage;
            request.target       = static_cast<uint32_t>(target);
            request.variant      = variant.entries.empty() ? nullptr : &variant;
            request.variantHash  = request.variant != nullptr ? variant.ContentHash() : 0;
            ShaderVariantSchema schema;
            std::string         error;
            if (!resolver.ResolveSchema(request.relativePath, request.target, schema, request.schemaFp, &error)) {
                return Invalid(error);
            }
            if (const auto invalid = MergeSpecialization(variant, schema, result.specialization)) {
                return invalid;
            }
            request.schema = &schema;
            ShaderCompileResult compiled;
            if (!resolver.ResolveShader(request, compiled, &error)) {
                return Invalid("Technique compile failed: " + error);
            }
            if (!compiled.reflection.pushConstants.empty())
                return Invalid("Technique push constants are not supported");
            if (!MergeShaderReflections(compiled.reflection, result.reflection, error))
                return Invalid(error);
            output.artifact = std::make_shared<const ShaderStageArtifact>(std::move(compiled));
            return {};
        }
    } // namespace

    std::variant<RendererError, RenderTechniqueData>
    CompileShaderTechnique(ShaderFileSystem &files, API backend, const ShaderTechnique &technique, IFileSystem *products)
    {
        const auto target = Target(backend);
        if (!target) {
            return RendererError{.code = RendererErrorCode::UNSUPPORTED, .message = "Technique backend is unsupported"};
        }
        if (const auto error = Validate(technique)) {
            return *error;
        }
        RenderTechniqueData result;
        result.backend = backend;
        ShaderResolver    resolver(products, nullptr, files, 0, 0, products ? ShaderResolveMode::PRODUCTS : ShaderResolveMode::DEVELOPMENT);
        const StageOutput stages[] = {{.stage = ShaderStageFlagBit::VS, .source = technique.vertex, .artifact = result.vertex},
                                      {.stage = ShaderStageFlagBit::FS, .source = technique.fragment, .artifact = result.fragment},
                                      {.stage = ShaderStageFlagBit::CS, .source = technique.compute, .artifact = result.compute}};
        for (const auto &stage : stages) {
            if (Present(stage.source)) {
                if (const auto error = CompileStage(resolver, *target, technique.variant, stage, result)) {
                    return *error;
                }
            }
        }
        return result;
    }
} // namespace sky::aurora
