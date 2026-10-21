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
        std::optional<RendererError> MergeReflection(const ShaderReflection &stage, ShaderReflection &merged)
        {
            if (!stage.pushConstants.empty()) {
                return Invalid("Technique push constants are not supported");
            }
            for (const auto &resource : stage.resources) {
                const auto found = std::ranges::find_if(
                    merged.resources, [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; });
                if (found == merged.resources.end()) {
                    merged.resources.push_back(resource);
                } else if (found->type != resource.type || found->count != resource.count || found->descriptorArray != resource.descriptorArray ||
                           found->texture != resource.texture || found->sampler != resource.sampler) {
                    return Invalid("Technique stages disagree on resource bindings");
                }
            }
            for (const auto &block : stage.blocks) {
                const auto found =
                    std::ranges::find_if(merged.blocks, [&](const auto &item) { return item.set == block.set && item.binding == block.binding; });
                if (found == merged.blocks.end()) {
                    merged.blocks.push_back(block);
                } else if (found->size != block.size || found->members != block.members) {
                    return Invalid("Technique stages disagree on uniform layout");
                }
            }
            return {};
        }
        struct StageOutput {
            ShaderStageFlagBit    stage;
            const ShaderRef      &source;
            std::vector<uint8_t> &code;
            std::string          &entry;
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
            output.entry = output.source.entry;
            output.code.resize(compiled.data.size() * sizeof(uint32_t));
            std::memcpy(output.code.data(), compiled.data.data(), output.code.size());
            if (output.stage == ShaderStageFlagBit::CS) {
                std::ranges::copy(compiled.reflection.threadGroupSize, result.reflection.threadGroupSize);
            }
            return MergeReflection(compiled.reflection, result.reflection);
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
        ShaderResolver    resolver(products, nullptr, files, 0, 0);
        const StageOutput stages[] = {
            {.stage = ShaderStageFlagBit::VS, .source = technique.vertex, .code = result.vertexCode, .entry = result.vertexEntry},
            {.stage = ShaderStageFlagBit::FS, .source = technique.fragment, .code = result.fragmentCode, .entry = result.fragmentEntry},
            {.stage = ShaderStageFlagBit::CS, .source = technique.compute, .code = result.computeCode, .entry = result.computeEntry}};
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
