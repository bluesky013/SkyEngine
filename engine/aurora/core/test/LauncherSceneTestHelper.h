#pragma once

#include <aurora/adaptor/ILauncherSceneSource.h>
#include <functional>
#include <utility>

namespace sky::aurora::test {
    class LauncherSceneSession final : public ILauncherSceneSession {
    public:
        using AppendFunction = std::function<void(FramePacket &, const LauncherSceneContext &)>;
        using EventFunction  = std::function<void(const RendererEvent &)>;

        explicit LauncherSceneSession(AppendFunction append, EventFunction event = {}) : append(std::move(append)), event(std::move(event))
        {
        }
        std::optional<RendererError> Append(FramePacket &packet, const LauncherSceneContext &context) override
        {
            if (append)
                append(packet, context);
            return {};
        }
        void OnEvent(const RendererEvent &value) override
        {
            if (event)
                event(value);
        }

    private:
        AppendFunction append;
        EventFunction  event;
    };
} // namespace sky::aurora::test
