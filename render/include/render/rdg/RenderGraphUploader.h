//
// Created by Zach Lee on 2023/9/8.
//

#pragma once

namespace sky::rdg {
    class RenderGraph;

    struct RenderGraphUploader {
        explicit RenderGraphUploader(RenderGraph &g) : rdg(g) {}

        [[maybe_unused]] void UploadConstantBuffers();

        RenderGraph &rdg;
    };

} // namespace sky::rdg
