#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <core/environment/Singleton.h>

#include <vector>

namespace sky::aurora {

    struct SceneExtensionRetirement {
        RenderFeatureId extension;
        RenderFeatureId scene;
    };

    // Modules request retirement; the host flush appends the command on its next packet.
    class SceneExtensionRetirementQueue : public Singleton<SceneExtensionRetirementQueue> {
        friend class Singleton<SceneExtensionRetirementQueue>;

    public:
        void Request(RenderFeatureId extension, RenderFeatureId scene)
        {
            if (extension.IsValid() && scene.IsValid())
                pending.push_back({extension, scene});
        }

        void Drain(std::vector<SceneExtensionRetirement> &out)
        {
            out.insert(out.end(), pending.begin(), pending.end());
            pending.clear();
        }

    private:
        SceneExtensionRetirementQueue() = default;
        std::vector<SceneExtensionRetirement> pending;
    };

} // namespace sky::aurora
