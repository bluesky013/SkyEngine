#pragma once

#include <aurora/renderer/IRenderFeature.h>
#include <aurora/renderer/RenderCommands.h>

#include <cstdint>
#include <optional>
#include <span>

namespace sky::aurora {

    class ISceneResourceSource;
    struct SceneExtensionSourceView {
        Uuid       world;
        ViewportId output;
        Vector3    cameraPosition{};
    };

    struct SceneExtensionSourceContext {
        RenderFeatureId                           scene;
        uint64_t                                  hostEpoch = 0;
        RendererInfo                              renderer;
        std::shared_ptr<ISceneResourceSource>     resources;
        std::span<const SceneExtensionSourceView> views;
    };

    // Appends atomically on the main thread. The host retains BUSY packets and stops its renderer on rejection.
    class ISceneRenderExtensionSource {
    public:
        virtual ~ISceneRenderExtensionSource()                                                          = default;
        virtual std::optional<RendererError> Append(FramePacket &, const SceneExtensionSourceContext &) = 0;
    };

} // namespace sky::aurora
