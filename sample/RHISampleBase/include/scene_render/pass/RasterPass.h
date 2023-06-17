//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <scene_render/pass/PassExecutor.h>

namespace sky::rhi {

    class RasterPass : public PassExecutor {
    public:
        RasterPass() = default;
        ~RasterPass() = default;

        void Execute(const CommandBufferPtr &cmd) override {}
    };

}