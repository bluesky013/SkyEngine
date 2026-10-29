//
// Created on 2026/10/29.
//

#pragma once

namespace sky::detail {
    template <typename Host>
    bool RunAndroidLauncherLoop(Host &host)
    {
        bool closing = false;
        while (!host.DestroyRequested()) {
            if (!closing) {
                if (host.RunApplicationStep())
                    continue;
                closing = true;
                if (!host.DestroyRequested())
                    host.RequestFinish();
                host.ShutdownApplication();
            }
            // finish() is asynchronous; glue callbacks still require native acknowledgements.
            if (!host.DestroyRequested() && !host.PollLifecycle())
                return false;
        }
        if (!closing)
            host.ShutdownApplication();
        return true;
    }
} // namespace sky::detail
