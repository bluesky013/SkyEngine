//
// Created by Zach on 2023/1/30.
//

#pragma once

#include <rhi/Device.h>
#include <gles/Context.h>
#include <gles/PBuffer.h>
#include <gles/Swapchain.h>
#include <gles/Image.h>
#include <gles/Queue.h>
#include <gles/RenderPass.h>
#include <gles/FrameBuffer.h>
#include <gles/CommandBuffer.h>
#include <gles/Fence.h>
#include <gles/Shader.h>
#include <gles/DescriptorSetLayout.h>
#include <gles/PipelineLayout.h>
#include <gles/VertexInput.h>
#include <memory>

namespace sky::gles {

    class Device : public rhi::Device {
    public:
        Device() = default;
        ~Device();

        template <typename T, typename Desc>
        inline std::shared_ptr<T> CreateDeviceObject(const Desc &des)
        {
            auto res = new T(*this);
            if (!res->Init(des)) {
                delete res;
                res = nullptr;
            }
            return std::shared_ptr<T>(res);
        }

        template <typename T, typename Desc>
        inline std::shared_ptr<T> CreateDescObject(const Desc &des)
        {
            auto res = std::make_shared<T>();
            if (!res->Init(des)) {
                res = nullptr;
            }
            return res;
        }

        bool Init(const Descriptor &desc);

        Context *GetMainContext() const;
        Queue *GetGraphicsQueue() const;
        Queue *GetTransferQueue() const;
        Queue* GetQueue(rhi::QueueType type) const override;

        // Device Object
        CREATE_DEV_OBJ(SwapChain)
        CREATE_DEV_OBJ(Image)
        CREATE_DEV_OBJ(RenderPass)
        CREATE_DEV_OBJ(FrameBuffer)
        CREATE_DEV_OBJ(CommandBuffer)
        CREATE_DEV_OBJ(Fence)
        CREATE_DEV_OBJ(Shader)
        CREATE_DEV_OBJ(GraphicsPipeline)
        CREATE_DEV_OBJ(DescriptorSetLayout)
        CREATE_DEV_OBJ(PipelineLayout)

        CREATE_DESC_OBJ(VertexInput)

    private:
        std::unique_ptr<Context> mainContext;
        std::unique_ptr<Queue> graphicsQueue;
        std::unique_ptr<Queue> transferQueue;
    };

}
