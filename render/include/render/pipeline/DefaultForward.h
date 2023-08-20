//
// Created by Zach Lee on 2023/8/20.
//

#pragma once

#include <render/RenderPipeline.h>

namespace sky {
    class RenderWindow;

    class DefaultForward : public RenderPipeline {
    public:
        DefaultForward() = default;
        ~DefaultForward() override = default;

        void SetOutput(RenderWindow *wnd);
        void OnSetup(rdg::RenderGraph &rdg) override;

    private:
        RenderWindow *output = nullptr;
    };

} // namespace sky