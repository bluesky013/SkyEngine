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

    class ImGuiInstance : public IWindowEvent {
    public:
        ImGuiInstance();
        ~ImGuiInstance() override;

        void AddWidget(ImWidget *widget);

        void Tick(float delta);
        void Render(rdg::RenderGraph &rdg);

        void MakeCurrent();
        void BindNativeWindow(const NativeWindow *window);

        void OnMouseMove(int32_t x, int32_t y) override;
        void OnMouseButtonDown(MouseButtonType button) override;
        void OnMouseButtonUp(MouseButtonType button) override;
        void OnMouseWheel(int32_t wheelX, int32_t wheelY) override;
        void OnFocusChanged(bool focus) override;
        void OnWindowResize(uint32_t width, uint32_t height) override;
        void OnTextInput(const char *text) override;
        void OnKeyUp(KeyButtonType) override;
        void OnKeyDown(KeyButtonType) override;

        RenderPrimitive *GetPrimitive() const { return primitive.get(); }
    private:
        void CheckVertexBuffers(uint32_t vertexCount, uint32_t indexCount);

        ImContext context;

        std::unique_ptr<RenderPrimitive> primitive;
        RDBufferPtr stagingBuffer;
        RDBufferPtr vertexBuffer;
        RDBufferPtr indexBuffer;
        RDResourceGroupPtr globalSet;

        RDTexture2DPtr fontTexture;
        RDDynamicUniformBufferPtr ubo;
        uint32_t uploadHandle = 0;
        uint64_t vertexSize = 0;
        uint64_t indexSize = 0;

        ImDrawData* drawData = nullptr;
        std::list<ImWidget*> widgets;
    };

} // namespace sky
