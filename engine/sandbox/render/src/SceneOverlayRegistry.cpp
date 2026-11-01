//
// Created on 2026/10/30.
//

#include <editor/render/SceneOverlayRegistry.h>

#include <algorithm>
#include <limits>

namespace sky::editor {

    bool SceneOverlayRegistry::Add(std::string id, std::shared_ptr<ISceneOverlay> overlay, std::shared_ptr<const void> lease)
    {
        if (id.empty() || !overlay || !lease || nextIndex == std::numeric_limits<uint32_t>::max() ||
            std::ranges::any_of(entries, [&](const auto &entry) { return entry.id == id && !entry.retiring; })) {
            return false;
        }
        entries.push_back({std::move(lease), std::move(overlay), std::move(id), nextIndex++, false});
        return true;
    }

    void SceneOverlayRegistry::Remove(const std::string &id)
    {
        for (auto &entry : entries) {
            if (entry.id == id)
                entry.retiring = true;
        }
    }

    void SceneOverlayRegistry::Append(aurora::FramePacket                                 &packet,
                                      aurora::SceneId                                      scene,
                                      World                                               *world,
                                      aurora::RendererInfo                                 info,
                                      const std::shared_ptr<aurora::ISceneResourceSource> &resources)
    {
        for (auto &entry : entries) {
            if (entry.retiring)
                entry.overlay->Retire(packet);
            else
                entry.overlay->Append(packet, {world, info, {scene, {entry.index, 1}}, resources});
        }
        std::erase_if(entries, [](const auto &entry) { return entry.retiring; });
    }

    void SceneOverlayRegistry::OnEvent(const aurora::RendererEvent &event)
    {
        for (const auto &entry : entries)
            entry.overlay->OnEvent(event);
    }

    void SceneOverlayRegistry::OnRendererStopped()
    {
        for (const auto &entry : entries)
            entry.overlay->OnRendererStopped();
        std::erase_if(entries, [](const auto &entry) { return entry.retiring; });
    }

} // namespace sky::editor
