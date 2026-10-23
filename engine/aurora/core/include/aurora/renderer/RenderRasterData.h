//
// Created on 2026/10/17.
//

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/Shader.h>
#include <aurora/rhi/ShaderParameters.h>
#include <aurora/rhi/ShaderReflection.h>
#include <cmath>
#include <core/name/Name.h>
#include <core/util/Uuid.h>
#include <map>
#include <set>
#include <tuple>

namespace sky::aurora {
    struct RenderResourceKey {
        Uuid     product;
        uint64_t revision                                    = 0;
        uint64_t usageVariant                                = 0;
        uint64_t deviceEpoch                                 = 0;
        bool     operator==(const RenderResourceKey &) const = default;
    };

    struct RenderMeshStream {
        uint64_t        offset = 0, size = 0;
        uint32_t        stride = 0;
        VertexInputRate rate   = VertexInputRate::PER_VERTEX;
    };
    struct RenderMeshLod {
        uint64_t                    offset = 0, size = 0;
        IndexType                   type = IndexType::U32;
        CmdDrawIndexed              command;
        std::vector<CmdDrawIndexed> sections;
    };
    struct RenderMeshData {
        std::vector<RenderMeshStream>      streams;
        std::vector<RasterVertexAttribute> attributes;
        std::vector<RenderMeshLod>         lods;
    };
    struct RenderTechniqueData {
        API                               backend = API::DEFAULT;
        ShaderStageArtifactPtr            vertex, fragment, compute;
        ShaderReflection                  reflection;
        std::vector<ShaderBindingPolicy>  bindingPolicies;
        std::vector<ShaderParameterAlias> parameterAliases;
        ShaderSpecialization              specialization;
        BlendState                        blend;
        RasterDepthState                  depth;
    };
    struct RenderMaterialValue {
        Name            name;
        ShaderDataValue value;
    };
    struct RenderMaterialImage {
        Name              name;
        RenderResourceKey resource;
    };
    struct RenderMaterialSampler {
        Name                name;
        Sampler::Descriptor descriptor;
    };
    struct RenderMaterialParameters {
        std::vector<RenderMaterialValue>   values;
        std::vector<RenderMaterialImage>   images;
        std::vector<RenderMaterialSampler> samplers;
    };
    struct RenderMaterialTechnique {
        Name                     tag;
        RenderTechniqueData      program;
        RenderMaterialParameters parameters;
    };
    inline bool IsValidMaterialSampler(const Sampler::Descriptor &value)
    {
        return value.minFilter <= Filter::LINEAR && value.magFilter <= Filter::LINEAR && value.mipmapMode <= MipFilter::LINEAR &&
               value.addressModeU <= WrapMode::MIRROR_CLAMP_TO_EDGE && value.addressModeV <= WrapMode::MIRROR_CLAMP_TO_EDGE &&
               value.addressModeW <= WrapMode::MIRROR_CLAMP_TO_EDGE && std::isfinite(value.minLod) && std::isfinite(value.maxLod) &&
               std::isfinite(value.maxAnisotropy) && value.minLod >= 0 && value.maxLod >= value.minLod && value.maxAnisotropy >= 1;
    }

    inline bool ValidateRenderMaterialParameters(const RenderTechniqueData      &program,
                                                 const RenderMaterialParameters &parameters,
                                                 uint64_t                        deviceEpoch,
                                                 std::string                    &error)
    {
        ShaderParameterTable table;
        if (!BuildShaderParameterTable(program.reflection, 1, program.parameterAliases, table, error))
            return false;
        std::set<std::tuple<uint32_t, uint32_t, std::string>>          targets;
        std::map<std::pair<uint32_t, uint32_t>, std::set<std::string>> fields;
        const auto resolve = [&](Name name, ShaderParameterKind kind, const ShaderParameterEntry *&entry) {
            ShaderParameterHandle handle;
            const auto            status = table.Resolve(name, 2, handle);
            if ((status != ShaderParameterLookup::READY && status != ShaderParameterLookup::INACTIVE) ||
                table.Resolve(handle, entry) == ShaderParameterLookup::STALE || !entry || entry->kind != kind ||
                !targets.emplace(entry->set, entry->binding, entry->field).second) {
                error = "Material parameter is unknown, ambiguous, duplicated, or has the wrong semantic kind";
                return false;
            }
            return true;
        };
        for (const auto &value : parameters.values) {
            const ShaderParameterEntry *entry = nullptr;
            if (!resolve(value.name, ShaderParameterKind::FIELD, entry) || !ValidateShaderData(value.value, error))
                return false;
            if (entry->dataType != value.value.type || entry->arrayCount != value.value.count) {
                error = "Material data has an incompatible semantic type or array count";
                return false;
            }
            fields[{entry->set, entry->binding}].insert(entry->field);
        }
        for (const auto &image : parameters.images) {
            const ShaderParameterEntry *entry = nullptr;
            if (!resolve(image.name, ShaderParameterKind::RESOURCE, entry))
                return false;
            const auto declaration =
                std::find_if(program.reflection.declarations.begin(), program.reflection.declarations.end(),
                             [&](const auto &resource) { return resource.set == entry->set && resource.binding == entry->binding; });
            if (entry->resourceType != ShaderResourceType::SAMPLED_IMAGE || declaration == program.reflection.declarations.end() ||
                declaration->count != 1 || declaration->descriptorArray || declaration->texture.dimension != ShaderTextureDimension::D2 ||
                declaration->texture.arrayed || declaration->texture.multisampled || declaration->texture.scalarType != ShaderScalarType::FLOAT ||
                declaration->texture.components != 4 || !image.resource.product || !image.resource.revision || !image.resource.usageVariant ||
                image.resource.deviceEpoch != deviceEpoch) {
                error = "Material image requires an exact ordinary float4 2D texture version on this device epoch";
                return false;
            }
        }
        for (const auto &sampler : parameters.samplers) {
            const ShaderParameterEntry *entry = nullptr;
            if (!resolve(sampler.name, ShaderParameterKind::RESOURCE, entry))
                return false;
            const auto declaration =
                std::find_if(program.reflection.declarations.begin(), program.reflection.declarations.end(),
                             [&](const auto &resource) { return resource.set == entry->set && resource.binding == entry->binding; });
            if (entry->resourceType != ShaderResourceType::SAMPLER || declaration == program.reflection.declarations.end() ||
                declaration->count != 1 || declaration->descriptorArray || declaration->sampler != ShaderSamplerType::REGULAR ||
                !IsValidMaterialSampler(sampler.descriptor)) {
                error = "Material sampler has an incompatible type or descriptor";
                return false;
            }
        }
        for (const auto &block : program.reflection.blocks) {
            const auto provided = fields.find({block.set, block.binding});
            if (provided != fields.end())
                for (const auto &member : block.members)
                    if (!provided->second.contains(member.name)) {
                        error = "Material data is missing a field of its owned uniform block";
                        return false;
                    }
        }
        return true;
    }

    struct RenderMaterialData {
        std::vector<RenderMaterialTechnique> techniques;
    };
} // namespace sky::aurora
