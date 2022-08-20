//
// Created by Zach Lee on 2022/8/19.
//

#pragma once

#include <render/resources/Shader.h>
#include <render/resources/Buffer.h>
#include <render/resources/Technique.h>
#include <render/resources/Texture.h>
#include <render/resources/DescirptorGroup.h>
#include <render/RenderFeature.h>
#include <render/imgui/GuiPrimitive.h>
#include <framework/window/IWindowEvent.h>
#include <core/util/Macros.h>
#include <imgui.h>

struct ImGuiContext;

namespace sky {

    class IGuiWidget {
    public:
        IGuiWidget() = default;
        virtual ~IGuiWidget() = default;

        SKY_DISABLE_COPY(IGuiWidget)

        virtual void OnRender(ImGuiContext* context) = 0;
    };

    class LambdaWidget : public IGuiWidget {
    public:
        template <typename T, typename = std::enable_if<!std::is_same_v<T, LambdaWidget>>>
        explicit LambdaWidget(T&& func) : executor(std::forward<T>(func)) {}
        ~LambdaWidget() override = default;

        void OnRender(ImGuiContext* context) override
        {
            if (executor) {
                executor(context);
            }
        }

    private:
        std::function<void(ImGuiContext*)> executor;
    };

    class GuiRenderer : public RenderFeature, public IWindowEvent {
    public:
        explicit GuiRenderer(RenderScene& scn) : RenderFeature(scn) {}
        ~GuiRenderer() override;

        void Init();

        void OnTick(float time) override;

        void GatherRenderPrimitives() override;

        void OnBindViewport(const RenderViewport& viewport) override;

        void OnViewportSizeChange(const RenderViewport& viewport) override;

        template <typename T, typename ...Args>
        T* Create(Args&& ...args)
        {
            auto widget = new T(std::forward<Args>(args)...);
            widgets.emplace_back(widget);
            return static_cast<T*>(widgets.back().get());
        }

        template <typename T>
        LambdaWidget* CreateLambda(T&& func)
        {
            return Create<LambdaWidget>(std::forward<T>(func));
        }

    private:
        void CheckBufferSize(uint64_t vertexSize, uint64_t indexSize);

        RDGfxShaderTablePtr shaders;
        RDGfxTechniquePtr technique;
        RDBufferPtr vertexBuffer;
        RDBufferPtr indexBuffer;
        std::unique_ptr<GuiPrimitive> primitive;
        std::unique_ptr<DescriptorPool> pool;
        uint64_t currentVertexSize = 0;
        uint64_t currentIndexSize = 0;
        uint32_t width = 1;
        uint32_t height = 1;
        bool inited = false;
        std::vector<std::unique_ptr<IGuiWidget>> widgets;
    };
}