//
// Created by Zach Lee on 2021/12/26.
//

#include <render/RenderPipeline.h>
#include <render/DriverManager.h>
#include <render/RenderScene.h>
#include <render/framegraph/FrameGraph.h>

namespace sky {

    RenderPipeline::~RenderPipeline()
    {
        DriverManager::Get()->GetDevice()->WaitIdle();
    }

    void RenderPipeline::DoFrame(FrameGraph& frameGraph, const drv::CommandBufferPtr& cmdBuffer)
    {
        frameGraph.Compile();

        auto& views = scene.GetViews();
        for (auto& view : views) {
            auto& primitives = view->GetPrimitives();
            for (auto& primitive : primitives) {
                for (auto& encoder : encoders) {
                    primitive->Encode(encoder);
                }
            }
        }
    }
}