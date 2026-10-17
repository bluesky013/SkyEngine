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
        std::variant<FrameError, RasterUniformBinding> Uniform(
            DeviceFrameContext &context, const DeviceFrameLease &lease, FrameGraphBuilder &builder, uint32_t set, std::span<const std::byte> bytes)
        {
            auto written = context.WriteFrameBuffer(lease, FrameBufferKind::UNIFORM, bytes);
            if (auto *error = std::get_if<FrameError>(&written))
                return *error;
            const auto &slice = std::get<FrameBufferSlice>(written);
            if (slice.offset > UINT32_MAX)
                return Invalid("Uniform offset exceeds the dynamic offset range");
            auto imported = builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
            if (auto *error = std::get_if<FrameError>(&imported))
                return *error;
            const bool dynamic = set != 1;
            return RasterUniformBinding{set,
                                        0,
                                        {std::get<GraphBufferHandle>(imported), dynamic ? 0 : slice.offset, slice.size},
                                        dynamic ? static_cast<uint32_t>(slice.offset) : 0};
        }
    } // namespace

    std::variant<FrameError, PreparedRasterView> ResolvePreparedRaster(Device                                 &device,
                                                                       DeviceFrameContext                     &context,
                                                                       const DeviceFrameLease                 &lease,
                                                                       FrameGraphBuilder                      &builder,
                                                                       const PreparedViewData                 &view,
                                                                       const FrozenResourceCatalog            &catalog,
                                                                       std::span<const PreparedMeshLayout>     meshes,
                                                                       std::span<const PreparedMaterialLayout> materials,
                                                                       Rect2D                                  scissor,
                                                                       std::span<const PreparedGraphBuffer>    inGraph)
    {
        if (lease.context != context.GetIdentity() || lease.context.backend != device.GetAPI())
            return Invalid("Prepared raster requires the active device context lease");
        using ResourceKey = decltype(Key({}));
        std::map<ResourceKey, std::shared_ptr<const ResourceVersionLease>> pins;
        std::map<ResourceKey, const PreparedMeshLayout *>                  meshLayouts;
        std::map<ResourceKey, const PreparedMaterialLayout *>              materialLayouts;
        for (const auto &pin : catalog.pins) {
            if (!pin || !pin->cpu || !pins.emplace(Key(pin->cpu->resource), pin).second)
                return Invalid("Invalid or duplicate pinned resource version");
        }
        for (const auto &mesh : meshes)
            if (!meshLayouts.emplace(Key(mesh.resource), &mesh).second)
                return Invalid("Duplicate mesh layout version");
        for (const auto &material : materials)
            if (!materialLayouts.emplace(Key(material.resource), &material).second)
                return Invalid("Duplicate material layout version");
        for (const auto &draw : view.draws) {
            const auto m       = meshLayouts.find(Key(draw.mesh.resource));
            const auto p       = materialLayouts.find(Key(draw.material.resource));
            const auto meshPin = pins.find(Key(draw.mesh.resource)), materialPin = pins.find(Key(draw.material.resource));
            if (draw.mesh.resource.deviceEpoch != lease.context.deviceEpoch || draw.material.resource.deviceEpoch != lease.context.deviceEpoch ||
                draw.entity.scene != view.view.scene || draw.subMesh != 0 || m == meshLayouts.end() || p == materialLayouts.end() ||
                meshPin == pins.end() || materialPin == pins.end() || draw.lod >= m->second->lods.size())
                return Invalid("Draw requires its exact prepared mesh, material and LOD");
            const auto &backing = meshPin->second->backing;
            const auto *buffer  = backing ? std::get_if<ResourceBufferBacking>(backing.get()) : nullptr;
            if (!buffer || !buffer->buffer || buffer->buffer->GetDevice() != &device || !p->second->shader ||
                p->second->shader->GetDevice() != &device || p->second->constants.empty())
                return Invalid("Prepared raster backing or shader is unavailable on this device");
            if (m->second->lods.size() != meshPin->second->cpu->metadata.lodDistances.size() + 1)
                return Invalid("Mesh layout and frozen LOD metadata disagree");
        }
        PreparedRasterView result;
        if (view.draws.empty())
            return result;
        const PreparedViewConstants constants{view.view.projection * view.view.view,
                                              {view.view.cameraPosition.x, view.view.cameraPosition.y, view.view.cameraPosition.z, 1}};
        auto                        global = Uniform(context, lease, builder, 0, std::as_bytes(std::span{&constants, 1}));
        if (auto *error = std::get_if<FrameError>(&global))
            return *error;
        std::map<ResourceKey, RasterUniformBinding> materialUniforms;
        std::map<ResourceKey, bool>                 retained;
        result.draws.reserve(view.draws.size());
        for (const auto &packet : view.draws) {
            const auto  meshKey = Key(packet.mesh.resource), materialKey = Key(packet.material.resource);
            const auto &mesh     = *meshLayouts.at(meshKey);
            const auto &material = *materialLayouts.at(materialKey);
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
            if (!materialUniforms.contains(materialKey)) {
                auto binding = Uniform(context, lease, builder, 1, material.constants);
                if (auto *error = std::get_if<FrameError>(&binding))
                    return *error;
                materialUniforms.emplace(materialKey, std::get<RasterUniformBinding>(binding));
            }
            const PreparedObjectConstants object{packet.world};
            auto                          binding = Uniform(context, lease, builder, 2, std::as_bytes(std::span{&object, 1}));
            if (auto *error = std::get_if<FrameError>(&binding))
                return *error;
            RasterDraw draw;
            draw.shader       = material.shader;
            draw.blend        = material.blend;
            draw.depth        = material.depth;
            draw.scissor      = scissor;
            draw.attributes   = mesh.attributes;
            const auto handle = std::get<GraphBufferHandle>(buffer);
            for (const auto &stream : mesh.streams)
                draw.vertices.push_back({{handle, stream.offset, stream.size}, stream.stride, stream.rate});
            const auto &lod = mesh.lods[packet.lod];
            draw.indexed    = RasterIndexedDraw{{handle, lod.offset, lod.size}, lod.type, lod.command};
            draw.uniforms   = {std::get<RasterUniformBinding>(global), materialUniforms.at(materialKey), std::get<RasterUniformBinding>(binding)};
            result.draws.push_back(std::move(draw));
            for (const auto &key : {meshKey, materialKey})
                if (retained.emplace(key, true).second)
                    result.pins.push_back(pins.at(key));
        }
        return result;
    }
} // namespace sky::aurora
