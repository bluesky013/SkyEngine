
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
#include <vulkan/Image.h>
#include <vulkan/ImageView.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/CommandPool.h>
#include <vulkan/Semaphore.h>
#include <core/logger/Logger.h>

static const char* TAG = "Render";

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

        drv::RenderPassFactory factory;
        auto pass = factory.operator()().AddSubPass()
            .AddColor()
                .ColorOp(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE)
                .Format(swapChain->GetFormat())
                .Layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            .AddDependency()
                .SetLinkage(VK_SUBPASS_EXTERNAL, 0)
                .SetBarrier(drv::Barrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT})
            .AddDependency()
                .SetLinkage(0, VK_SUBPASS_EXTERNAL)
                .SetBarrier(drv::Barrier{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_MEMORY_READ_BIT})
            .Create(*device);

        auto ext = swapChain->GetExtent();
        drv::FrameBuffer::Descriptor des = {
            ext.width,
            ext.height,
            pass,
            {swapChain->GetCurrentImageView()->GetNativeHandle()}
        };

        auto fb = device->CreateDeviceObject<drv::FrameBuffer>(des);

        auto queue = device->GetQueue({VK_QUEUE_GRAPHICS_BIT});

        drv::CommandPool::Descriptor poolDes = {
            queue->GetQueueFamilyIndex(),
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        };
        auto pool = device->CreateDeviceObject<drv::CommandPool>(poolDes);

        drv::Semaphore::Descriptor sDes = {};

        auto sem = device->CreateDeviceObject<drv::Semaphore>(sDes);
        auto cmd = pool->Allocate(drv::CommandBuffer::Descriptor{});

        cmd->Begin();

        VkClearValue clear;
        clear.color.float32[0] = 1.f;
        clear.color.float32[1] = 0.f;
        clear.color.float32[2] = 0.f;
        clear.color.float32[3] = 0.f;

        VkRenderPassBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.renderPass = pass->GetNativeHandle();
        beginInfo.framebuffer = fb->GetNativeHandle();
        beginInfo.renderArea = {{0, 0}, ext};
        beginInfo.clearValueCount = 1;
        beginInfo.pClearValues = &clear;

        vkCmdBeginRenderPass(cmd->GetNativeHandle(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(cmd->GetNativeHandle());

        cmd->End();

        drv::CommandBuffer::SubmitInfo submit = {
            {},
            {sem}
        };
        cmd->Submit(*queue, submit);

        drv::SwapChain::PresentInfo present = {
            {sem}
        };
        swapChain->Present(present);
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

    void Render::OnTick(float time)
    {
    }


}