//
// Created by blues on 2025/10/3.
//

#pragma once

#include <rhi/Device.h>
#include <framework/window/IWindowEvent.h>

#include <utility>

namespace sky {

    class ImWidget {
    public:
        explicit ImWidget(std::string name_) : name(std::move(name_)) {}
        virtual ~ImWidget() = default;

        virtual void Execute() = 0;

        inline const std::string &GetName() const { return name; }

    protected:
        std::string name;
    };

    class GuiRender : public IWindowEvent, public IMouseEvent {
    public:
        GuiRender(rhi::Device* dev, NativeWindow* window, const rhi::RenderPassPtr &pass);
        ~GuiRender() override;

        void Tick(float delta);
        void Render(rhi::GraphicsEncoder & encoder);

        void AddWidget(ImWidget* widget)
        {
            widgets.emplace_back(widget);
        }

        void RemoveWidget(ImWidget* widget)
        {
            widgets.erase(std::remove(widgets.begin(), widgets.end(), widget), widgets.end());
        }

    private:
        void CheckSize(uint32_t vertexCount, uint32_t indexCount);
        void OnWindowResize(const WindowResizeEvent& event) override;
        void OnFocusChanged(bool focus) override;

        void OnMouseButtonDown(const MouseButtonEvent &event) override;
        void OnMouseButtonUp(const MouseButtonEvent &event) override;
        void OnMouseMotion(const MouseMotionEvent &event) override;
        void OnMouseWheel(const MouseWheelEvent &event) override;

        rhi::Device* device = nullptr;

        rhi::GraphicsPipelinePtr pso;
        rhi::PipelineLayoutPtr layout;
        rhi::DescriptorSetPoolPtr setPool;
        rhi::DescriptorSetPtr set;

        rhi::BufferPtr ubo;
        rhi::ImagePtr image;
        rhi::ImageViewPtr font;
        rhi::BufferPtr vertexBuffer;
        rhi::BufferPtr indexBuffer;
        uint32_t vertexSize = 0;
        uint32_t indexSize = 0;

        EventBinder<IWindowEvent> binder;
        EventBinder<IMouseEvent> mouseBinder;
        std::vector<ImWidget*> widgets;
    };

} // namespace sky

