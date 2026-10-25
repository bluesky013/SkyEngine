//
// Created on 2026/10/16.
//

#include "graph/PreparedRaster.h"

#include "graph/FrameGraphBuilder.h"
#include "resource/ResourceCatalog.h"
#include <algorithm>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rhi/Device.h>
#include <map>
#include <cctype>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        auto Key(const RenderResourceKey &key)
        {
            return std::tuple{key.product, key.usageVariant, key.deviceEpoch, key.revision};
        }
        FrameError Invalid(const char *message)
        {
            return {FrameErrorCode::INVALID_ARGUMENT, message};
        }
    } // namespace

    std::variant<FrameError, std::vector<RasterVertexAttribute>>
    ResolveVertexInputs(std::span<const RasterVertexAttribute> attributes, const ShaderReflection &reflection)
    {
        if (attributes.size() > 16 || reflection.vertexInputs.size() > 16)
            return Invalid("Vertex input budget exceeded");
        std::vector<RasterVertexAttribute> result;
        std::set<uint32_t> locations;
        for (const auto &input : reflection.vertexInputs) {
            const RasterVertexAttribute *matched = nullptr;
            for (const auto &attribute : attributes) {
                const bool sameName = attribute.semantic.size() == input.semantic.size() &&
                    std::equal(attribute.semantic.begin(), attribute.semantic.end(), input.semantic.begin(),
                               [](unsigned char a, unsigned char b) { return std::toupper(a) == std::toupper(b); });
                if (sameName && attribute.semanticIndex == input.semanticIndex) {
                    if (matched)
                        return Invalid("Mesh has duplicate shader input semantics");
                    matched = &attribute;
                }
            }
            if (!matched || !locations.insert(input.location).second)
                return Invalid("Mesh is missing a shader input or shader locations overlap");
            const auto format = static_cast<uint32_t>(matched->format);
            const auto scalar = matched->format >= Format::F_R32 && matched->format <= Format::F_RGBA8 ? ShaderScalarType::FLOAT
                              : matched->format >= Format::U_R8 && matched->format <= Format::U_RGBA32 ? ShaderScalarType::UINT
                                                                                                     : ShaderScalarType::UNKNOWN;
            const auto components = format ? (format - 1) % 4 + 1 : 0;
            if (scalar != input.scalarType || components != input.components)
                return Invalid("Mesh vertex format does not match the shader input type");
            result.push_back(*matched);
            result.back().location = input.location;
        }
        return result;
    }

    namespace {
        bool SameVariant(const ShaderVariant &a, const ShaderVariant &b)
        {
            if (a.entries.size() != b.entries.size())
                return false;
            std::map<Name, uint32_t> values;
            for (const auto &entry : a.entries)
                values[entry.key] = entry.value;
            for (const auto &entry : b.entries) {
                const auto found = values.find(entry.key);
                if (found == values.end() || found->second != entry.value)
                    return false;
            }
            return true;
        }
    } // namespace

    const PreparedMaterialLayout *FindMaterialVariant(std::span<const PreparedMaterialLayout> materials,
                                                      const RenderResourceKey                &resource,
                                                      bool                                    skinned,
                                                      const ShaderVariant                    &pipelineValues,
                                                      uint16_t                                vertexMask)
    {
        for (const auto &candidate : materials)
            if (candidate.resource == resource && candidate.skinned == skinned &&
                (candidate.pipelineAgnostic || !vertexMask || candidate.vertexMask == vertexMask) &&
                (candidate.pipelineAgnostic || SameVariant(candidate.pipeline, pipelineValues)))
                return &candidate;
        return nullptr;
    }

    std::variant<FrameError, PreparedRasterView> ResolvePreparedRaster(Device                                 &device,
                                                                       DeviceFrameContext                     &context,
                                                                       const DeviceFrameLease                 &lease,
                                                                       FrameGraphBuilder                      &builder,
                                                                       const PreparedViewData                 &view,
                                                                       const FrozenResourceCatalog            &catalog,
                                                                       std::span<const PreparedMeshLayout>     meshes,
                                                                       std::span<const PreparedMaterialLayout> materials,
                                                                       Rect2D                                  scissor,
                                                                       std::span<const PreparedGraphBuffer>    inGraph,
                                                                       PreparedDrawList                        list,
                                                                       const ShaderVariant                    &pipelineValues)
    {
        if (lease.context != context.GetIdentity() || lease.context.backend != device.GetAPI())
            return Invalid("Prepared raster requires the active device context lease");
        if (list != PreparedDrawList::VISIBLE && list != PreparedDrawList::SHADOW_CASTERS)
            return Invalid("Unknown prepared draw list");
        const auto &packets = list == PreparedDrawList::VISIBLE ? view.draws : view.shadowCasters;
        using ResourceKey   = decltype(Key({}));
        std::map<ResourceKey, std::shared_ptr<const ResourceVersionLease>> pins;
        std::map<ResourceKey, const PreparedMeshLayout *> meshLayouts;
        for (const auto &pin : catalog.pins) {
            if (!pin || !pin->cpu || !pins.emplace(Key(pin->cpu->resource), pin).second)
                return Invalid("Invalid or duplicate pinned resource version");
        }
        for (const auto &mesh : meshes)
            if (!meshLayouts.emplace(Key(mesh.resource), &mesh).second)
                return Invalid("Duplicate mesh layout version");
        const auto selectMaterial = [&](const PreparedDrawPacket &draw) -> const PreparedMaterialLayout * {
            const auto mesh = meshLayouts.find(Key(draw.mesh.resource));
            if (mesh == meshLayouts.end())
                return nullptr;
            return FindMaterialVariant(materials, draw.material.resource, draw.skinning != UINT32_MAX, pipelineValues,
                                       RenderVertexMask(mesh->second->streams, mesh->second->attributes).mask);
        };
        for (const auto &draw : packets) {
            if (draw.skinning != UINT32_MAX && (!view.skinning || draw.skinning >= view.skinning->size() || !(*view.skinning)[draw.skinning])) {
                return Invalid("Prepared draw has an invalid skinning snapshot index");
            }
            const auto  m          = meshLayouts.find(Key(draw.mesh.resource));
            const auto *material    = selectMaterial(draw);
            const auto  meshPin     = pins.find(Key(draw.mesh.resource)), materialPin = pins.find(Key(draw.material.resource));
            if (draw.mesh.resource.deviceEpoch != lease.context.deviceEpoch || draw.material.resource.deviceEpoch != lease.context.deviceEpoch ||
                draw.entity.scene != view.view.scene || m == meshLayouts.end() || !material || meshPin == pins.end() ||
                materialPin == pins.end() || draw.lod >= m->second->lods.size())
                return Invalid("Draw requires its exact prepared mesh, material variant and LOD");
            const auto &backing = meshPin->second->backing;
            const auto *buffer  = backing ? std::get_if<ResourceBufferBacking>(backing.get()) : nullptr;
            if (!buffer || !buffer->buffer || buffer->buffer->GetDevice() != &device || !material->shader ||
                material->shader->GetDevice() != &device)
                return Invalid("Prepared raster backing or shader is unavailable on this device");
            for (const auto &image : material->parameters.images) {
                ShaderParameterHandle handle;
                if (material->shader->GetParameterTable().Resolve(image.name, 2, handle) == ShaderParameterLookup::INACTIVE)
                    continue;
                const auto  imagePin     = pins.find(Key(image.resource));
                const auto *imageBacking = imagePin != pins.end() && imagePin->second->backing
                                               ? std::get_if<ResourceImageBacking>(imagePin->second->backing.get())
                                               : nullptr;
                if (image.resource.deviceEpoch != lease.context.deviceEpoch || !imageBacking || !imageBacking->image ||
                    imageBacking->image->GetDevice() != &device || imageBacking->descriptor.imageType != ImageType::IMAGE_2D ||
                    imageBacking->descriptor.arrayLayers != 1 || imageBacking->descriptor.samples != SampleCount::X1 ||
                    !imageBacking->descriptor.usage.TestBit(ImageUsageFlagBit::SAMPLED))
                    return Invalid("Material requires its exact ordinary 2D sampled image backing on this device");
            }
            if (m->second->lods.size() != meshPin->second->cpu->metadata.lodDistances.size() + 1)
                return Invalid("Mesh layout and frozen LOD metadata disagree");
            const auto size       = buffer->buffer->GetSize();
            const auto validRange = [size](uint64_t offset, uint64_t range) { return range && offset <= size && range <= size - offset; };
            for (const auto &stream : m->second->streams)
                if (!stream.stride || !validRange(stream.offset, stream.size) || stream.size < stream.stride)
                    return Invalid("Prepared vertex stream exceeds its backing");
            const auto &lod = m->second->lods[draw.lod];
            if ((lod.sections.empty() && draw.subMesh != 0) || (!lod.sections.empty() && draw.subMesh >= lod.sections.size()))
                return Invalid("Draw requires a valid mesh section");
            if (!validRange(lod.offset, lod.size))
                return Invalid("Prepared index range exceeds its backing");
            const auto    &command   = lod.sections.empty() ? lod.command : lod.sections[draw.subMesh];
            const uint64_t indexSize = lod.type == IndexType::U16 ? 2 : 4;
            if ((lod.type != IndexType::U16 && lod.type != IndexType::U32) || lod.offset % indexSize || !command.indexCount ||
                !command.instanceCount || uint64_t(command.firstIndex) + command.indexCount > lod.size / indexSize)
                return Invalid("Prepared mesh section exceeds its index range");
        }
        PreparedRasterView          result;
        std::map<ResourceKey, bool> retained;
        result.draws.reserve(packets.size());
        for (const auto &packet : packets) {
            const auto  meshKey = Key(packet.mesh.resource), materialKey = Key(packet.material.resource);
            const auto &mesh     = *meshLayouts.at(meshKey);
            const auto *material = selectMaterial(packet);
            if (!material)
                return Invalid("Draw requires its exact material variant for this Pass/View");
            const auto &backing  = std::get<ResourceBufferBacking>(*pins.at(meshKey)->backing);
            auto        initial  = backing.state;
            if (backing.tracking) {
                const auto tracked = context.GetResourceState(FrameResource{backing.buffer});
                if (const auto *error = std::get_if<FrameError>(&tracked))
                    return *error;
                initial = std::get<CopyBufferState>(std::get<FrameResourceStateSnapshot>(tracked).accepted.state);
            }
            const auto local  = std::find_if(inGraph.begin(), inGraph.end(), [&](const auto &item) { return item.resource == packet.mesh.resource; });
            auto       buffer = local != inGraph.end() ? std::variant<FrameError, GraphBufferHandle>{local->handle}
                                                       : builder.Import(backing.buffer, initial, backing.state, true);
            if (auto *error = std::get_if<FrameError>(&buffer))
                return *error;
            RasterDraw draw;
            draw.shader       = material->shader;
            draw.blend        = material->blend;
            draw.depth        = material->depth;
            draw.scissor      = scissor;
            auto attributes = ResolveVertexInputs(mesh.attributes, material->shader->GetReflection());
            if (const auto *error = std::get_if<FrameError>(&attributes))
                return *error;
            draw.attributes = std::move(std::get<std::vector<RasterVertexAttribute>>(attributes));
            const auto handle = std::get<GraphBufferHandle>(buffer);
            for (const auto &stream : mesh.streams)
                draw.vertices.push_back({{handle, stream.offset, stream.size}, stream.stride, stream.rate});
            const auto &lod = mesh.lods[packet.lod];
            draw.indexed =
                RasterIndexedDraw{{handle, lod.offset, lod.size}, lod.type, lod.sections.empty() ? lod.command : lod.sections[packet.subMesh]};
            result.draws.push_back({std::move(draw), packet.world, material->parameters, packet.receiveShadow, packet.entity});
            if (packet.skinning != UINT32_MAX) {
                result.draws.back().skinning = (*view.skinning)[packet.skinning];
            }
            for (const auto &image : material->parameters.images) {
                const auto key = Key(image.resource);
                if (pins.contains(key) && retained.emplace(key, true).second)
                    result.pins.push_back(pins.at(key));
            }
            for (const auto &key : {meshKey, materialKey})
                if (retained.emplace(key, true).second)
                    result.pins.push_back(pins.at(key));
        }
        return result;
    }
} // namespace sky::aurora
