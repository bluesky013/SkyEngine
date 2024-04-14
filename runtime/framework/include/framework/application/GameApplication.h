//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <framework/application/Application.h>
#include <framework/window/NativeWindow.h>
#include <core/file/FileSystem.h>

namespace sky {

    class GameApplication : public Application {
    public:
        GameApplication() = default;
        ~GameApplication() override = default;

        bool Init(int argc, char **argv) override;
        void PreInit() override;
        void PostInit() override;

        void Shutdown() override;
        void PreTick() override;

        const NativeWindow *GetViewport() const override;

    private:
        void LoadConfigs() override;

        std::unique_ptr<NativeWindow> nativeWindow;
        uint32_t width = 1024;
        uint32_t height = 1024;

        FileSystemPtr workFs;
    };

}
