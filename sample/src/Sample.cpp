//
// Created by Zach Lee on 2021/11/26.
//

#include <Sample.h>
#include <render/DriverManager.h>
#include <render/framegraph/FrameGraph.h>
#include <render/framegraph/FrameGraphPass.h>

namespace sky {

    void Sample::Init()
    {
    }

    void Sample::Start()
    {
        DriverManager::Get()->Initialize({"Sample"});

        FrameGraph graph;
        graph.AddPass<FrameGraphGraphicPass>([](FrameGraphBuilder& builder) {

        });
    }

    void Sample::Stop()
    {
        DriverManager::Get()->ShutDown();
        DriverManager::Destroy();
    }

    void Sample::Tick(float delta)
    {

    }

}