//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <list>
#include <rhi/CommandBuffer.h>
#include <gles/egl/Context.h>

#include <gles/DevObject.h>
#include <gles/CommandStorage.h>
#include <gles/Fence.h>
#include <gles/CommandContext.h>

namespace sky::gles {
    class Device;
    class CommandBuffer;

    class GraphicsEncoder : public rhi::GraphicsEncoder {
    public:
        GraphicsEncoder(CommandBuffer &cmd, CommandContext *ctx) : commandBuffer(cmd), context(ctx) {}
        ~GraphicsEncoder() = default;

        rhi::GraphicsEncoder &BeginPass(const rhi::PassBeginInfo &beginInfo) override;
        rhi::GraphicsEncoder &BindPipeline(const rhi::GraphicsPipelinePtr &pso) override;
        rhi::GraphicsEncoder &BindAssembly(const rhi::VertexAssemblyPtr &assembly) override;
        rhi::GraphicsEncoder &SetViewport(uint32_t count, const rhi::Viewport *viewport) override;
        rhi::GraphicsEncoder &SetScissor(uint32_t count, const rhi::Rect2D *scissor) override;
        rhi::GraphicsEncoder &DrawIndexed(const rhi::CmdDrawIndexed &indexed) override;
        rhi::GraphicsEncoder &DrawLinear(const rhi::CmdDrawLinear &linear) override;
        rhi::GraphicsEncoder &DrawIndexedIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) override;
        rhi::GraphicsEncoder &DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) override;
        rhi::GraphicsEncoder &BindSet(uint32_t id, const rhi::DescriptorSetPtr &set) override;
        rhi::GraphicsEncoder &NextSubPass() override;
        rhi::GraphicsEncoder &EndPass() override;

    private:
        CommandBuffer &commandBuffer;
        CommandContext *context = nullptr;
    };

    class BlitEncoder : public rhi::BlitEncoder {
    public:
        BlitEncoder(CommandBuffer &cmd, CommandContext *ctx) : cmdBuffer(cmd), context(ctx) {}
        ~BlitEncoder() = default;

        rhi::BlitEncoder &CopyTexture() override;
        rhi::BlitEncoder &CopyTextureToBuffer() override;
        rhi::BlitEncoder &CopyBufferToTexture() override;
        rhi::BlitEncoder &BlitTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::BlitInfo> &blitInputs, rhi::Filter filter) override;
        rhi::BlitEncoder &ResoleTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::ResolveInfo> &resolves) override;

    private:
        friend class CommandBuffer;
        CommandBuffer        &cmdBuffer;
        CommandContext *context = nullptr;
    };


    class CommandBuffer : public rhi::CommandBuffer, public DevObject {
    public:
        CommandBuffer(Device &dev) : DevObject(dev) {}
        ~CommandBuffer() noexcept;

        void Begin() override;
        void End() override;
        void Submit(rhi::Queue &queue, const rhi::SubmitInfo &info) override;
        std::shared_ptr<rhi::GraphicsEncoder> EncodeGraphics() override;
        std::shared_ptr<rhi::BlitEncoder> EncodeBlit() override;

        struct TaskBase {
            TaskBase() = default;
            virtual ~TaskBase() = default;
            virtual void Execute() {};
            TaskBase* next = nullptr;
        };

        template <typename Func, typename ...Args>
        class Task : public TaskBase {
        public:
            using Parameters = std::tuple<std::remove_reference_t<Args>...>;
            using FuncType = Func;

            Task(Func &&f, Args &&...args) : func(f), params(std::forward<Args>(args)...) {}

            FuncType   func;
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

    private:
        friend class GraphicsEncoder;
        using StoragePtr = std::unique_ptr<CommandStorage>;
        using Iterator = std::list<StoragePtr>::iterator;

        void AllocateStorage();
        void Reset();
        void Execute();

        std::list<StoragePtr> storages;
        Iterator iterator = storages.end();
        TaskBase* head = nullptr;
        TaskBase** current = &head;
        std::unique_ptr<CommandContext> context;
    };

}
