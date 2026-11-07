#pragma once

#include <aurora/adaptor/renderer/ISceneRenderExtensionSource.h>
#include <core/environment/Singleton.h>

#include <algorithm>
#include <optional>
#include <vector>

namespace sky::aurora {

    // Process-wide list of scene extension sources. Sources are appended in registration
    // order and only register/unregister during module lifecycle, never from Append.
    class SceneRenderExtensionSourceRegistry : public Singleton<SceneRenderExtensionSourceRegistry> {
        friend class Singleton<SceneRenderExtensionSourceRegistry>;

    public:
        bool Register(ISceneRenderExtensionSource &source)
        {
            if (std::find(sources.begin(), sources.end(), &source) != sources.end())
                return false;
            sources.push_back(&source);
            return true;
        }

        void UnRegister(ISceneRenderExtensionSource &source)
        {
            sources.erase(std::remove(sources.begin(), sources.end(), &source), sources.end());
        }

        bool Empty() const
        {
            return sources.empty();
        }

        std::optional<RendererError> Append(FramePacket &packet, const SceneExtensionSourceContext &context) const
        {
            for (auto *source : sources) {
                if (auto error = source->Append(packet, context))
                    return error;
            }
            return {};
        }

    private:
        SceneRenderExtensionSourceRegistry() = default;

        std::vector<ISceneRenderExtensionSource *> sources;
    };

} // namespace sky::aurora
