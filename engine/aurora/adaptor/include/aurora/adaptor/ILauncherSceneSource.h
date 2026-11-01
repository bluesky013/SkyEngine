#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <memory>
#include <optional>

namespace sky::aurora {

    struct LauncherSceneContext {
        SceneId    scene;
        ViewId     mainView;
        ViewportId output;
        Extent2D   extent;
    };

    class ILauncherSceneSession {
    public:
        virtual ~ILauncherSceneSession() = default;
        // Append owned render data only; simulation ticks independently of renderer backpressure.
        virtual std::optional<RendererError> Append(FramePacket &, const LauncherSceneContext &) = 0;
        virtual void                         OnEvent(const RendererEvent &)                      = 0;
    };

    struct LauncherSceneRegistration {
        Uuid     host;
        uint64_t generation                                          = 0;
        bool     operator==(const LauncherSceneRegistration &) const = default;
    };

    class ILauncherSceneSource {
    public:
        virtual ~ILauncherSceneSource() = default;

        // Main-thread only. The host retains the session through every in-progress call.
        // Replacement starts a new scene after the previous registration's exact retirement.
        virtual std::variant<RendererError, LauncherSceneRegistration> RegisterSceneSession(std::shared_ptr<ILauncherSceneSession> session) = 0;
        virtual std::optional<RendererError>                           UnregisterSceneSession(LauncherSceneRegistration registration)       = 0;
    };

} // namespace sky::aurora
