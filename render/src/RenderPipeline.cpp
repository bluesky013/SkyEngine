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
                auto& techniques = primitive->GetTechniques();
                for (auto& tech : techniques) {
                    for (auto& encoder : encoders) {
                        if ((encoder->GetDrawTag() & tech->drawTag) == 0) {
                            continue;
                        }
                        drv::DrawItem item;
                        item.pso = tech->pso;
                        item.vertexAssembly = tech->assembly;
                        item.drawArgs = tech->args;
                        item.shaderResources = tech->setBinder;
                        encoder->Emplace(item);
                    }
                }
            }
        }
    }
}