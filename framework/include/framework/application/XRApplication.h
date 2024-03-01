//
// Created by blues on 2024/3/13.
//

#pragma once

#include <framework/application/Application.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    class XRApplication : public Application {
    public:
        XRApplication() = default;
        ~XRApplication() override = default;

        bool Init(int argc, char **argv) override;
        void Shutdown() override;
    private:
        void LoadConfigs() override;

        std::unique_ptr<NativeWindow> nativeWindow;
        uint32_t width = 1024;
        uint32_t height = 1024;
    };

} // namespace sky