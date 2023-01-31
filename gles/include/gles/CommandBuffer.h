//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <list>
#include <rhi/CommandBuffer.h>
#include <gles/DevObject.h>
#include <gles/CommandStorage.h>

namespace sky::gles {
    class Device;
    class CommandBuffer;

    class GraphicsEncoder : public rhi::GraphicsEncoder {
    public:
        GraphicsEncoder(CommandBuffer &cmd) : commandBuffer(cmd) {}
        ~GraphicsEncoder() = default;

        void BeginPass(const rhi::FrameBufferPtr &frameBuffer, const rhi::RenderPassPtr &renderPass, uint32_t clearCount, rhi::ClearValue *clearValues);
        void BindPipeline(const rhi::GraphicsPipelinePtr &pso);
        void BindAssembly(const rhi::VertexAssemblyPtr &assembly);
        void SetViewport(uint32_t count, const rhi::Viewport *viewport);
        void SetScissor(uint32_t count, const rhi::Rect2D *scissor);
        void DrawIndexed(const rhi::CmdDrawIndexed &indexed);
        void DrawLinear(const rhi::CmdDrawLinear &linear);
        void DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size);
        void EndPass();

    private:
        CommandBuffer &commandBuffer;
    };

    class CommandBuffer : public rhi::CommandBuffer, public DevObject {
    public:
        CommandBuffer(Device &dev) : DevObject(dev) {}
        ~CommandBuffer() = default;

        void Reset();

        std::shared_ptr<rhi::GraphicsEncoder> EncodeGraphics() override;

        struct TaskBase {
            TaskBase() = default;
            virtual ~TaskBase() = default;
            virtual void Execute() {};
            TaskBase* next = nullptr;
        };

        template <typename Func, typename ...Args>
        class Task : public TaskBase {
        public:
            using Parameters = std::tuple<Args...>;

            Task(Func &&f, Args &&...args) : func(f), params(std::forward<Args>(args)...) {}

            Func func;
            Parameters params;

            void Execute() override
            {
                std::apply(func, params);
            }
        };

        template <typename Func, typename ...Args>
        void EnqueueMessage(Func &&func, Args &&...args)
        {
            using TaskType = Task<Func, Args...>;
            uint8_t *ptr = Allocate(sizeof(TaskType));
            TaskType *task = new (ptr) TaskType(std::forward<Func>(func), std::forward<Args>(args)...);

            (*current) = task;
            current = &(task->next);
        }

        uint8_t* Allocate(uint32_t size);

        bool Init(const Descriptor &desc);

        void Execute();

    private:
        friend class GraphicsEncoder;
        using StoragePtr = std::unique_ptr<CommandStorage>;
        using Iterator = std::list<StoragePtr>::iterator;

        void AllocateStorage();

        std::list<StoragePtr> storages;
        Iterator iterator;
        TaskBase* head = nullptr;
        TaskBase** current = &head;
    };

}
