//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <vulkan/CommandBuffer.h>
#include <core/util/Macros.h>

namespace sky {

    class RenderEncoder {
    public:
        RenderEncoder() = default;
        virtual ~RenderEncoder() = default;

        virtual void Encode(drv::GraphicsEncoder& encoder) {}

        virtual bool MultiThreadEncode() const { return false; }
    };

    class DrawCallProducer {
    public:
        DrawCallProducer() = default;
        virtual ~DrawCallProducer() = default;

        virtual void Process(drv::GraphicsEncoder& encoder) = 0;

        SKY_DISABLE_COPY(DrawCallProducer)
    };

    class ItemDrawCallProducer : public DrawCallProducer {
    public:
        ItemDrawCallProducer(const drv::DrawItem& v) : item(v) {}
        ~ItemDrawCallProducer() override = default;

        void Process(drv::GraphicsEncoder& encoder) override;

    private:
        drv::DrawItem item;
    };

    template <typename T>
    class LambdaDrawCallProducer : public DrawCallProducer {
    public:
        LambdaDrawCallProducer(T&& func) : function(std::forward<T>(func)) {}
        ~LambdaDrawCallProducer() = default;

        void Process(drv::GraphicsEncoder& encoder) override
        {
            function(encoder);
        }

    private:
        std::function<void(drv::GraphicsEncoder& encoder)> function;
    };

    class RenderRasterEncoder : public RenderEncoder {
    public:
        RenderRasterEncoder() = default;
        ~RenderRasterEncoder() = default;

        void Encode(drv::GraphicsEncoder& encoder) override;

        void Emplace(const drv::DrawItem& item);

        bool MultiThreadEncode() const override;

        template <typename T>
        void EmplaceLambda(T&& func)
        {
            producers.emplace_back(new LambdaDrawCallProducer<T>(std::forward<T>(func)));
        }

        void SetDrawTag(uint32_t tag);

        uint32_t GetDrawTag() const;

    private:
        using ProducerPtr = std::unique_ptr<DrawCallProducer>;

        uint32_t drawTag {0};
        std::vector<ProducerPtr> producers;
    };
}
