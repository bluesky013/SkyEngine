//
// Created on 2026/10/16.
//

#include "graph/PreparedRaster.h"

#include "graph/FrameGraphBuilder.h"
#include "resource/ResourceCatalog.h"
#include <algorithm>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rhi/Device.h>
#include <cctype>
#include <map>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        FrameError Invalid(const char *message)
        {
            return {FrameErrorCode::INVALID_ARGUMENT, message};
        }
    } // namespace

    std::variant<FrameError, std::vector<RasterVertexAttribute>> ResolveVertexInputs(std::span<const RasterVertexAttribute> attributes,
                                                                                     const ShaderReflection                &reflection)
    {
        if (attributes.size() > 16 || reflection.vertexInputs.size() > 16)
            return Invalid("Vertex input budget exceeded");
        std::vector<RasterVertexAttribute> result;
        std::set<uint32_t>                 locations;
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
            const auto format     = static_cast<uint32_t>(matched->format);
            const auto scalar     = matched->format >= Format::F_R32 && matched->format <= Format::F_RGBA8   ? ShaderScalarType::FLOAT
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
        bool MatchesVariant(const PreparedMaterialLayout &candidate, bool skinned, const ShaderVariant &pipelineValues, uint16_t vertexMask)
        {
            return candidate.skinned == skinned && (candidate.pipelineAgnostic || !vertexMask || candidate.vertexMask == vertexMask) &&
                   (candidate.pipelineAgnostic || SameVariant(candidate.pipeline, pipelineValues));
        }
    } // namespace

    const PreparedMaterialLayout *FindMaterialVariant(std::span<const PreparedMaterialLayout> materials,
                                                      const RenderResourceKey                &resource,
                                                      bool                                    skinned,
                                                      const ShaderVariant                    &pipelineValues,
                                                      uint16_t                                vertexMask)
    {
        for (const auto &candidate : materials)
            if (candidate.resource == resource && MatchesVariant(candidate, skinned, pipelineValues, vertexMask))
                return &candidate;
        return nullptr;
    }

    bool PreparedRasterIndex::GeometryReady(const PreparedDrawPacket &draw) const
    {
        const auto ready = [&](const RenderResourceKey &key) {
            const auto found = pins.find(Key(key));
            return found != pins.end() && found->second->backing;
        };
        return ready(draw.mesh.resource) && ready(draw.material.resource) && meshes.contains(Key(draw.mesh.resource)) &&
               materials.contains(Key(draw.material.resource));
    }

    std::variant<FrameError, PreparedRasterIndex> BuildPreparedRasterIndex(const FrozenResourceCatalog                &catalog,
                                                                           std::span<const PreparedMeshLayout>         meshes,
                                                                           std::span<const PreparedMaterialTechniques> materials,
                                                                           std::span<const PreparedGraphBuffer>        inGraph)
    {
        PreparedRasterIndex index;
        for (const auto &pin : catalog.pins)
            if (!pin || !pin->cpu || !index.pins.emplace(PreparedRasterIndex::Key(pin->cpu->resource), pin).second)
                return Invalid("Invalid or duplicate pinned resource version");
        for (const auto &mesh : meshes)
            if (!index.meshes.emplace(PreparedRasterIndex::Key(mesh.resource), &mesh).second)
                return Invalid("Duplicate mesh layout version");
        for (const auto &material : materials) {
            const auto [entry, inserted] = index.materials.try_emplace(PreparedRasterIndex::Key(material.resource));
            if (!inserted)
                return Invalid("Duplicate material layout version");
            for (const auto &[tag, layout] : material.techniques) {
                if (tag.Empty() || layout.resource != material.resource)
                    return Invalid("Material technique requires its owning resource version and Tag");
                entry->second[tag].push_back(&layout);
            }
        }
        for (const auto &buffer : inGraph)
            if (!index.localBuffers.emplace(PreparedRasterIndex::Key(buffer.resource), buffer.handle).second)
                return Invalid("Duplicate graph-local buffer version");
        return index;
    }

    std::variant<FrameError, PreparedRasterView> ResolvePreparedRaster(Device                                                    &device,
                                                                       DeviceFrameContext                                        &context,
                                                                       const DeviceFrameLease                                    &lease,
                                                                       FrameGraphBuilder                                         &builder,
                                                                       SceneId                                                    scene,
                                                                       std::span<const PreparedDrawPacket>                        packets,
                                                                       std::span<const std::shared_ptr<const RenderSkinningData>> skinning,
                                                                       const PreparedRasterIndex                                 &index,
                                                                       Name                                                       tag,
                                                                       Rect2D                                                     scissor,
                                                                       const ShaderVariant                                       &pipelineValues)
    {
        if (lease.context != context.GetIdentity() || lease.context.backend != device.GetAPI())
            return Invalid("Prepared raster requires the active device context lease");
        if (tag.Empty())
            return Invalid("Prepared raster requires a queue Tag");
        struct Selection {
            const PreparedDrawPacket          *packet;
            const PreparedMeshLayout          *mesh;
            const PreparedMaterialLayout      *material;
            std::vector<RasterVertexAttribute> attributes;
        };
        std::vector<Selection> selected;
        selected.reserve(packets.size());
        for (const auto &draw : packets) {
            const auto materialEntry = index.materials.find(PreparedRasterIndex::Key(draw.material.resource));
            if (materialEntry == index.materials.end())
                return Invalid("Draw requires its exact prepared material version");
            const auto queue = materialEntry->second.find(tag);
            if (queue == materialEntry->second.end())
                continue;
            if (draw.skinning != UINT32_MAX && (draw.skinning >= skinning.size() || !skinning[draw.skinning]))
                return Invalid("Prepared draw has an invalid skinning snapshot index");
            const auto meshEntry   = index.meshes.find(PreparedRasterIndex::Key(draw.mesh.resource));
            const auto meshPin     = index.pins.find(PreparedRasterIndex::Key(draw.mesh.resource));
            const auto materialPin = index.pins.find(PreparedRasterIndex::Key(draw.material.resource));
            if (draw.mesh.resource.deviceEpoch != lease.context.deviceEpoch || draw.material.resource.deviceEpoch != lease.context.deviceEpoch ||
                draw.entity.scene != scene || meshEntry == index.meshes.end() || meshPin == index.pins.end() || materialPin == index.pins.end() ||
                draw.lod >= meshEntry->second->lods.size())
                return Invalid("Draw requires its exact prepared mesh, material and LOD");
            const auto                   &mesh       = *meshEntry->second;
            const auto                    vertexMask = RenderVertexMask(mesh.streams, mesh.attributes).mask;
            const PreparedMaterialLayout *material   = nullptr;
            for (const auto *candidate : queue->second)
                if (MatchesVariant(*candidate, draw.skinning != UINT32_MAX, pipelineValues, vertexMask)) {
                    if (material)
                        return Invalid("Draw has ambiguous material variants for this Pass/View");
                    material = candidate;
                }
            if (!material)
                return Invalid("Draw requires its exact material variant for this Pass/View");
            const auto &backing = meshPin->second->backing;
            const auto *buffer  = backing ? std::get_if<ResourceBufferBacking>(backing.get()) : nullptr;
            if (!buffer || !buffer->buffer || buffer->buffer->GetDevice() != &device || !material->shader || material->shader->GetDevice() != &device)
                return Invalid("Prepared raster backing or shader is unavailable on this device");
            auto attributes = ResolveVertexInputs(mesh.attributes, material->shader->GetReflection());
            if (const auto *error = std::get_if<FrameError>(&attributes))
                return *error;
            bool imagesReady = true;
            for (const auto &image : material->parameters.images) {
                ShaderParameterHandle handle;
                const auto            lookup = material->shader->GetParameterTable().Resolve(image.name, 2, handle);
                if (lookup == ShaderParameterLookup::INACTIVE)
                    continue;
                if (lookup != ShaderParameterLookup::READY || image.resource.deviceEpoch != lease.context.deviceEpoch)
                    return Invalid("Material requires a valid image parameter and device epoch");
                const auto imagePin = index.pins.find(PreparedRasterIndex::Key(image.resource));
                if (imagePin == index.pins.end() || !imagePin->second->backing) {
                    imagesReady = false;
                    continue;
                }
                const auto *imageBacking = std::get_if<ResourceImageBacking>(imagePin->second->backing.get());
                if (!imageBacking || !imageBacking->image || imageBacking->image->GetDevice() != &device ||
                    imageBacking->descriptor.imageType != ImageType::IMAGE_2D || imageBacking->descriptor.arrayLayers != 1 ||
                    imageBacking->descriptor.samples != SampleCount::X1 || !imageBacking->descriptor.usage.TestBit(ImageUsageFlagBit::SAMPLED))
                    return Invalid("Material requires its exact ordinary 2D sampled image backing on this device");
            }
            if (mesh.lods.size() != meshPin->second->cpu->metadata.lodDistances.size() + 1)
                return Invalid("Mesh layout and frozen LOD metadata disagree");
            const auto size       = buffer->buffer->GetSize();
            const auto validRange = [size](uint64_t offset, uint64_t range) { return range && offset <= size && range <= size - offset; };
            for (const auto &stream : mesh.streams)
                if (!stream.stride || !validRange(stream.offset, stream.size) || stream.size < stream.stride)
                    return Invalid("Prepared vertex stream exceeds its backing");
            const auto &lod = mesh.lods[draw.lod];
            if ((lod.sections.empty() && draw.subMesh != 0) || (!lod.sections.empty() && draw.subMesh >= lod.sections.size()))
                return Invalid("Draw requires a valid mesh section");
            if (!validRange(lod.offset, lod.size))
                return Invalid("Prepared index range exceeds its backing");
            const auto    &command   = lod.sections.empty() ? lod.command : lod.sections[draw.subMesh];
            const uint64_t indexSize = lod.type == IndexType::U16 ? 2 : 4;
            if ((lod.type != IndexType::U16 && lod.type != IndexType::U32) || lod.offset % indexSize || !command.indexCount ||
                !command.instanceCount || uint64_t(command.firstIndex) + command.indexCount > lod.size / indexSize)
                return Invalid("Prepared mesh section exceeds its index range");
            if (imagesReady)
                selected.push_back({&draw, &mesh, material, std::move(std::get<std::vector<RasterVertexAttribute>>(attributes))});
        }
        PreparedRasterView                         result;
        std::set<PreparedRasterIndex::ResourceKey> retained;
        result.draws.reserve(selected.size());
        for (auto &[packet, mesh, material, attributes] : selected) {
            const auto  meshKey     = PreparedRasterIndex::Key(packet->mesh.resource);
            const auto  materialKey = PreparedRasterIndex::Key(packet->material.resource);
            const auto &backing     = std::get<ResourceBufferBacking>(*index.pins.at(meshKey)->backing);
            auto        initial     = backing.state;
            if (backing.tracking) {
                const auto tracked = context.GetResourceState(FrameResource{backing.buffer});
                if (const auto *error = std::get_if<FrameError>(&tracked))
                    return *error;
                initial = std::get<CopyBufferState>(std::get<FrameResourceStateSnapshot>(tracked).accepted.state);
            }
            const auto local  = index.localBuffers.find(meshKey);
            auto       buffer = local != index.localBuffers.end() ? std::variant<FrameError, GraphBufferHandle>{local->second}
                                                                  : builder.Import(backing.buffer, initial, backing.state, true);
            if (auto *error = std::get_if<FrameError>(&buffer))
                return *error;
            RasterDraw draw;
            draw.shader       = material->shader;
            draw.blend        = material->blend;
            draw.depth        = material->depth;
            draw.scissor      = scissor;
            draw.attributes   = std::move(attributes);
            const auto handle = std::get<GraphBufferHandle>(buffer);
            for (const auto &stream : mesh->streams)
                draw.vertices.push_back({{handle, stream.offset, stream.size}, stream.stride, stream.rate});
            const auto &lod = mesh->lods[packet->lod];
            draw.indexed =
                RasterIndexedDraw{{handle, lod.offset, lod.size}, lod.type, lod.sections.empty() ? lod.command : lod.sections[packet->subMesh]};
            result.draws.push_back({std::move(draw), packet->world, material->parameters, packet->receiveShadow, packet->entity});
            if (packet->skinning != UINT32_MAX)
                result.draws.back().skinning = skinning[packet->skinning];
            for (const auto &image : material->parameters.images) {
                const auto key = PreparedRasterIndex::Key(image.resource);
                if (index.pins.contains(key) && retained.insert(key).second)
                    result.pins.push_back(index.pins.at(key));
            }
            for (const auto &key : {meshKey, materialKey})
                if (retained.insert(key).second)
                    result.pins.push_back(index.pins.at(key));
        }
        return result;
    }
} // namespace sky::aurora
