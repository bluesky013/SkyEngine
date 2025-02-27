//
// Created by blues on 2023/9/20.
//

#pragma once

#include <imgui.h>
//#include <implot.h>

#include <render/resource/Texture.h>
#include <framework/window/IWindowEvent.h>
#include <render/RenderPrimitive.h>
#include <imgui/ImWidget.h>
#include <string>

namespace sky {
    namespace rdg {
        struct RenderGraph;
    } // namespace rdg
    class RenderScene;

    struct ImguiPrimitive : public RenderPrimitive {
        RDTexture2DPtr fontTexture;
        RDDynamicUniformBufferPtr ubo;

        void UpdateBatch() override;
    };

    class ImGuiInstance : public IWindowEvent, public IMouseEvent, public IKeyboardEvent {
    public:
        ImGuiInstance();
        ~ImGuiInstance() override;

        void AddWidget(ImWidget *widget);
        void RemoveWidget(ImWidget *widget);

        void Tick(float delta);
        void Render(rdg::RenderGraph &rdg);

        void MakeCurrent();
        void BindNativeWindow(const NativeWindow *window);

        RenderPrimitive *GetPrimitive() const { return primitive.get(); }
    private:
        void CheckVertexBuffers(uint32_t vertexCount, uint32_t indexCount);

        void OnMouseButtonDown(const MouseButtonEvent &event) override;
        void OnMouseButtonUp(const MouseButtonEvent &event) override;
        void OnMouseMotion(const MouseMotionEvent &event) override;
        void OnMouseWheel(const MouseWheelEvent &event) override;

        void OnKeyUp(const KeyboardEvent &event) override;
        void OnKeyDown(const KeyboardEvent &event) override;
        void OnTextInput(WindowID winID, const char *text) override;

        void OnFocusChanged(bool focus) override;
        void OnWindowResize(uint32_t width, uint32_t height) override;

        EventBinder<IWindowEvent> winBinder;
        EventBinder<IKeyboardEvent> keyBinder;
        EventBinder<IMouseEvent> mouseBinder;

        ImContext context;

        std::unique_ptr<ImguiPrimitive> primitive;
        RDDynamicBuffer vertexBuffer;
        RDDynamicBuffer indexBuffer;
        RDResourceGroupPtr globalSet;

        uint32_t vertexSize = 0;
        uint32_t indexSize = 0;

        ImDrawData* drawData = nullptr;
        std::list<ImWidget*> widgets;
    };

} // namespace sky
