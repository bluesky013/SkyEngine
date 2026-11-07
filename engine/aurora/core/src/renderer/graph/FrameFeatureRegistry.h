//
// Created on 2026/10/17.
//

#pragma once

#include <aurora/renderer/IRenderFeature.h>
#include <map>

namespace sky::aurora {
    struct FrameFeatureLease {
        RegisterRenderFeature registration;
        bool                  enabled    = true;
        uint64_t              activation = 1;
    };
    struct FrameFeatureUse {
        std::shared_ptr<FrameFeatureLease> lease;
        uint64_t                           activation = 0;
        bool                               CanBuild() const
        {
            return lease->enabled && lease->activation == activation;
        }
    };
    struct SceneExtensionLease {
        RegisterSceneExtension             registration;
        std::shared_ptr<FrameFeatureLease> scene;
        bool                               enabled = true;
    };
    struct SceneExtensionUse {
        std::shared_ptr<SceneExtensionLease> lease;
        uint64_t                             activation = 0;
        bool                                 CanBuild() const
        {
            return lease->enabled && lease->scene->enabled && lease->scene->activation == activation;
        }
    };
    class FrameFeatureRegistry {
    public:
        std::optional<RendererError>   Validate(const FramePacket &) const;
        void                           Apply(FramePacket &);
        std::vector<FrameFeatureUse>   Snapshot() const;
        std::vector<SceneExtensionUse> SnapshotExtensions() const;
        std::vector<RenderFeatureId>   CollectRetired();
        void                           Shutdown();

    private:
        struct Entry {
            uint32_t                           generation = 0;
            std::shared_ptr<FrameFeatureLease> lease;
            bool                               retiring = false;
        };
        struct ExtensionEntry {
            uint32_t                             generation = 0;
            std::shared_ptr<SceneExtensionLease> lease;
            bool                                 retiring = false;
        };
        std::map<uint32_t, Entry>          entries;
        std::map<uint32_t, ExtensionEntry> extensions;
        std::vector<uint32_t>              extensionOrder;
    };
} // namespace sky::aurora
