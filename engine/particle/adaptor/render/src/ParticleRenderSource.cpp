#include <particle/render/ParticleRenderFeature.h>

#include <aurora/adaptor/renderer/SceneAssetSource.h>

#include <particle/render/ParticleRenderPayload.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace sky::particle {
    void ParticleRenderSource::Submit(std::shared_ptr<const ParticleRenderFrame> frame)
    {
        if (frame && frame->world)
            frames[frame->world] = std::move(frame);
    }
    void ParticleRenderSource::RequestRetire()
    {
        if (registeredScene.IsValid())
            aurora::SceneExtensionRetirementQueue::Get()->Request(id, registeredScene);
    }
    void ParticleRenderSource::RemoveWorld(const Uuid &world)
    {
        frames.erase(world);
        views.erase(world);
    }
    bool ParticleRenderSource::GetView(const Uuid &world, ParticleViewInfo &value) const
    {
        const auto found = views.find(world);
        if (found == views.end())
            return false;
        value = found->second;
        return value.valid;
    }
    std::optional<aurora::RendererError>
    ParticleRenderSource::BuildAdditions(const aurora::SceneExtensionSourceContext          &context,
                                         std::span<const aurora::RenderResourceKey>         previous,
                                         aurora::FramePacket                               &additions,
                                         std::unordered_map<Uuid, ParticleViewInfo>         &nextViews,
                                         std::vector<aurora::RenderResourceKey>             &used)
    {
        std::vector<ParticleRenderTexture> textures;
        for (const auto &view : context.views) {
            if (!view.world || !view.output.IsValid() || !std::isfinite(view.cameraPosition.x) || !std::isfinite(view.cameraPosition.y) ||
                !std::isfinite(view.cameraPosition.z))
                return aurora::RendererError{aurora::RendererErrorCode::INVALID_ARGUMENT, "Particle source view is invalid"};
            nextViews[view.world] = {view.cameraPosition, true};
            const auto found      = frames.find(view.world);
            if (found == frames.end())
                continue;
            for (const auto &emitter : found->second->emitters) {
                if (emitter.type != ParticleRendererType::SPRITE || emitter.instances.empty() ||
                    std::any_of(textures.begin(), textures.end(), [&](const auto &entry) { return entry.id == emitter.texture; }))
                    continue;
                std::shared_ptr<const aurora::RenderResourcePayload> payload;
                if (emitter.texture) {
                    if (!context.resources)
                        return aurora::RendererError{aurora::RendererErrorCode::NOT_READY, "Particle image source is unavailable"};
                    auto resolved = context.resources->ResolveImage(emitter.texture, context.renderer, aurora::SceneImageUsage::MATERIAL);
                    if (const auto *error = std::get_if<aurora::RendererError>(&resolved))
                        return *error;
                    const auto *ready = std::get_if<std::shared_ptr<const aurora::RenderResourcePayload>>(&resolved);
                    if (!ready || !*ready)
                        continue;
                    payload = *ready;
                } else {
                    if (!whitePayload || whiteDeviceEpoch != context.renderer.deviceEpoch) {
                        auto white            = std::make_shared<aurora::RenderResourcePayload>();
                        white->resource       = {whiteId, 1, 3, context.renderer.deviceEpoch};
                        white->format         = Name("material-image");
                        white->metadata.image = {1, 1, 1, 1, aurora::RenderImageFormat::RGBA8_UNORM};
                        white->bytes          = {255, 255, 255, 255};
                        whitePayload          = std::move(white);
                        whiteDeviceEpoch      = context.renderer.deviceEpoch;
                    }
                    payload = whitePayload;
                }
                textures.push_back({emitter.texture, payload->resource});
                if (std::find(used.begin(), used.end(), payload->resource) == used.end()) {
                    used.push_back(payload->resource);
                    if (std::find(previous.begin(), previous.end(), payload->resource) == previous.end())
                        additions.commands.push_back(aurora::ResourceCpuReady{*payload});
                }
            }
            auto encoded = EncodeParticleRenderFrame(*found->second, view.output, textures);
            if (const auto *error = std::get_if<aurora::RendererError>(&encoded))
                return *error;
            auto payload = std::move(std::get<aurora::FrameExtensionPayload>(encoded));
            if (!payload.resources.empty())
                additions.extensions.push_back(std::move(payload));
        }
        return {};
    }

    void ParticleRenderSource::MergeAdditions(aurora::FramePacket &packet, aurora::FramePacket &&additions)
    {
        packet.commands.insert(packet.commands.end(), std::make_move_iterator(additions.commands.begin()),
                               std::make_move_iterator(additions.commands.end()));
        packet.extensions.insert(packet.extensions.end(), std::make_move_iterator(additions.extensions.begin()),
                                 std::make_move_iterator(additions.extensions.end()));
        packet.featureCommands.insert(packet.featureCommands.end(), std::make_move_iterator(additions.featureCommands.begin()),
                                      std::make_move_iterator(additions.featureCommands.end()));
        packet.releasedResources.insert(packet.releasedResources.end(), additions.releasedResources.begin(), additions.releasedResources.end());
    }

    std::optional<aurora::RendererError> ParticleRenderSource::Append(aurora::FramePacket &packet, const aurora::SceneExtensionSourceContext &context)
    {
        if (!feature || !context.renderer.deviceEpoch || !context.scene.IsValid())
            return aurora::RendererError{aurora::RendererErrorCode::NOT_READY, "Particle scene source requires an active renderer"};
        const bool newSession =
            context.scene != registeredScene || context.hostEpoch != registeredEpoch || context.renderer.deviceEpoch != registeredDeviceEpoch;
        const std::span<const aurora::RenderResourceKey> previous = newSession ? std::span<const aurora::RenderResourceKey>{} : published;
        aurora::FramePacket additions;
        if (newSession)
            additions.featureCommands.push_back(aurora::RegisterSceneExtension{id, context.scene, feature});
        std::unordered_map<Uuid, ParticleViewInfo> nextViews;
        std::vector<aurora::RenderResourceKey>     used;
        if (auto error = BuildAdditions(context, previous, additions, nextViews, used))
            return error;
        for (const auto &key : previous)
            if (std::find(used.begin(), used.end(), key) == used.end())
                additions.releasedResources.push_back(key);
        MergeAdditions(packet, std::move(additions));
        views                 = std::move(nextViews);
        published             = std::move(used);
        registeredScene       = context.scene;
        registeredEpoch       = context.hostEpoch;
        registeredDeviceEpoch = context.renderer.deviceEpoch;
        return {};
    }

} // namespace sky::particle
