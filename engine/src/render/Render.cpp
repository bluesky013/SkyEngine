
//
// Created by Zach Lee on 2021/11/12.
//

#include <engine/render/Render.h>
#include <engine/world/World.h>
#include <engine/world/Viewport.h>
#include <engine/render/RenderScene.h>
#include <vulkan/Driver.h>
#include <vulkan/RenderPass.h>
#include <vulkan/Swapchain.h>

namespace sky {

    Render::~Render()
    {
        if (device != nullptr) {
            delete device;
            device = nullptr;
        }
        drv::Driver::Destroy(driver);
    }

    bool Render::Init(const StartInfo& info)
    {
        drv::Driver::Descriptor drvDes = {};
        drvDes.appName = "SkyEngine";
        drvDes.enableDebugLayer = true;
        drvDes.appName = info.appName;
        driver = drv::Driver::Create(drvDes);
        if (driver == nullptr) {
            return false;
        }

        drv::Device::Descriptor devDes = {};
        device = driver->CreateDevice(devDes);
        if (device == nullptr) {
            return false;
        }

//        drv::RenderPassFactory factory;
//        auto pass = factory.operator()().AddSubPass()
//            .AddColor()
//                .ColorOp(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE)
//                .Format(VK_FORMAT_R8G8B8A8_UNORM)
//                .Layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
//            .AddDependency()
//                .SetLinkage(VK_SUBPASS_EXTERNAL, 0)
//                .SetBarrier(drv::Barrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//                    VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT})
//            .AddDependency()
//                .SetLinkage(0, VK_SUBPASS_EXTERNAL)
//                .SetBarrier(drv::Barrier{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
//                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_MEMORY_READ_BIT})
//            .Create(*device);

        return true;
    }

    void Render::OnAddWorld(World& world)
    {
        auto iter = scenes.find(&world);
        if (iter != scenes.end()) {
            return;
        }
        auto scene = new RenderScene(*this);
        scenes.emplace(&world, scene);
    }

    void Render::OnRemoveWorld(World& world)
    {
        auto iter = scenes.find(&world);
        if (iter == scenes.end()) {
            return;
        }
        delete iter->second;
        scenes.erase(iter);
    }

    void Render::OnAddViewport(Viewport& vp)
    {
        auto iter = swapChains.find(&vp);
        if (iter != swapChains.end()) {
            return;
        }
        drv::SwapChain::Descriptor swcDes = {};
        swcDes.window = vp.GetNativeWindow();
        auto swapChain = device->CreateDeviceObject<drv::SwapChain>(swcDes);
        swapChains.emplace(&vp, swapChain);
    }

    void Render::OnRemoveViewport(Viewport& vp)
    {
        auto iter = swapChains.find(&vp);
        if (iter == swapChains.end()) {
            return;
        }
        delete iter->second;
        swapChains.erase(iter);
    }

    void Render::OnWorldTargetChange(World& world, Viewport& vp)
    {
        auto sIt = scenes.find(&world);
        if (sIt == scenes.end()) {
            return;
        }

        auto vIt = swapChains.find(&vp);
        if (vIt == swapChains.end()) {
            return;
        }

        sIt->second->SetTarget(*vIt->second);
    }


}