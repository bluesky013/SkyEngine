
//
// Created by Zach Lee on 2021/11/12.
//

#include <render/Render.h>
#include <world/World.h>
#include <world/Viewport.h>
#include <vulkan/Driver.h>
#include <vulkan/RenderPass.h>
#include <vulkan/SwapChain.h>
#include <render/RenderScene.h>

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
        auto scene = new RenderScene();
        world.RegisterWorldListener(scene);
        scenes.emplace(&world, scene);
    }

    void Render::OnRemoveWorld(World& world)
    {
        auto iter = scenes.find(&world);
        if (iter == scenes.end()) {
            return;
        }
        world.UnRegisterWorldListener(iter->second);
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


}