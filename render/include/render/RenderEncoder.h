//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <core/util/Macros.h>
#include <functional>
#include <memory>
#include <vector>
#include <vulkan/CommandBuffer.h>

namespace sky {

    class RenderEncoder {
    public:
        RenderEncoder()          = default;
        virtual ~RenderEncoder() = default;

        virtual void Encode(vk::GraphicsEncoder &encoder)
        {
        }

        virtual bool MultiThreadEncode() const
        {
            return false;
        }
    };

    class DrawCallProducer {
    public:
        DrawCallProducer()          = default;
        virtual ~DrawCallProducer() = default;

        virtual void Process(vk::GraphicsEncoder &encoder) = 0;

        SKY_DISABLE_COPY(DrawCallProducer)
    };

    class ItemDrawCallProducer : public DrawCallProducer {
    public:
        ItemDrawCallProducer(const vk::DrawItem &v) : item(v)
        {
        }
        ~ItemDrawCallProducer() override = default;

        void Process(vk::GraphicsEncoder &encoder) override;

    private:
        vk::DrawItem item;
    };

    template <typename T>
    class LambdaDrawCallProducer : public DrawCallProducer {
    public:
        LambdaDrawCallProducer(T &&func) : function(std::forward<T>(func))
        {
        }
        ~LambdaDrawCallProducer() = default;

        void Process(vk::GraphicsEncoder &encoder) override
        {
            function(encoder);
        }

    private:
        std::function<void(vk::GraphicsEncoder &encoder)> function;
    };

    class RenderRasterEncoder : public RenderEncoder {
    public:
        RenderRasterEncoder()  = default;
        ~RenderRasterEncoder() = default;

        void Encode(vk::GraphicsEncoder &encoder) override;

        void Emplace(const vk::DrawItem &item);

        bool MultiThreadEncode() const override;

        template <typename T>
        void EmplaceLambda(T &&func)
        {
            producers.emplace_back(new LambdaDrawCallProducer<T>(std::forward<T>(func)));
        }

        void SetDrawTag(uint32_t tag);

        uint32_t GetDrawTag() const;

    private:
        using ProducerPtr = std::unique_ptr<DrawCallProducer>;

        uint32_t                 drawTag{0};
        std::vector<ProducerPtr> producers;
    };
} // namespace sky
