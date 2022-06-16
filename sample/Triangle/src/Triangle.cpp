//
// Created by Zach Lee on 2022/6/16.
//


#include <Triangle.h>
#include <render/DriverManager.h>
#include <render/framegraph/FrameGraph.h>
#include <render/framegraph/FrameGraphPass.h>

namespace sky {

    void Triangle::Init()
    {
    }

    void Triangle::Start()
    {
        DriverManager::Get()->Initialize({"Sample"});

        FrameGraph graph;
        graph.AddPass<FrameGraphGraphicPass>([](FrameGraphBuilder& builder) {

        });
    }

    void Triangle::Stop()
    {
        DriverManager::Get()->ShutDown();
        DriverManager::Destroy();
    }

    void Triangle::Tick(float delta)
    {

    }

}