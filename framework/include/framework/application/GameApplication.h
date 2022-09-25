//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <framework/application/Application.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    class GameApplication : public Application {
    public:
        GameApplication() = default;
        ~GameApplication() = default;

        bool Init(StartInfo &) override;
        void Shutdown() override;
        void PreTick() override;

        const NativeWindow *GetViewport() const;

    private:
        std::unique_ptr<NativeWindow> nativeWindow;
    };

}
