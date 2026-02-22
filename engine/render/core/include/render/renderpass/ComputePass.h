//
// Created by blues on 2024/9/3.
//

#pragma once

#include <render/renderpass/PassBase.h>

namespace sky {

    class ComputePass : public PassBase {
    public:
        explicit ComputePass(const Name &name_) : PassBase(name_) {}
        ~ComputePass() override = default;

    private:
        RDGfxTechPtr technique;
    };

} // namespace sky