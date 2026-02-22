//
// Created by blues on 2025/10/3.
//

#pragma once

#include "GuiRender.h"
#include <rhi/Device.h>
#include <core/math/Vector2.h>
#include <core/event/Event.h>
#include <core/crypto/md5/MD5.h>
#include <framework/window/NativeWindow.h>
#include <builder/render/image/ImageProcess.h>

namespace sky {

    struct ImageUbo {
        Vector2 screenExt;
        Vector2 imageExt;
        Vector2 scale;
        Vector2 translate;
        uint32_t mipLevel;
    };

    struct ImageQuadInstance {
        Vector2 offset;
        uint32_t mip;
        uint32_t slice;
    };

    class ImageRender : public ImWidget, public IWindowEvent, public IMouseEvent, public IKeyboardEvent {
    public:
        ImageRender(rhi::Device* dev, NativeWindow* window, const rhi::RenderPassPtr &pass);
        ~ImageRender() override;

        void OnDropFile(const std::string& payload);
        void Render(rhi::GraphicsEncoder & encoder);

        void LoadConfigs();
        void SaveConfigs();

    private:
        // window event
        void OnWindowResize(const WindowResizeEvent& event) override;

        // mouse event
        void OnMouseWheel(const MouseWheelEvent &event) override;
        void OnMouseButtonDown(const MouseButtonEvent &event) override;
        void OnMouseButtonUp(const MouseButtonEvent &event) override;
        void OnMouseMotion(const MouseMotionEvent &event) override;

        // keyboard event
        void OnTextInput(WindowID windID, const char *text) override;

        void Execute() override;
        void DrawPresetSettingsPopup();
        void UpdateImageExtent(uint32_t width, uint32_t height);

        void GenerateMipMap();
        void UpdateImage(const builder::ImageObjectPtr& image);
        void UpdateCompressedImage(const std::string& key, const builder::CompressedImagePtr& image);
        void CompressImage(const std::string& key, const builder::CompressOption& op);
        void ShowOriginalImage();
        void ShowCompressImage(const std::string& key);

        rhi::Device* device = nullptr;

        rhi::GraphicsPipelinePtr pso;
        rhi::PipelineLayoutPtr layout;
        rhi::DescriptorSetPoolPtr setPool;
        rhi::DescriptorSetPtr set;

        rhi::BufferPtr ubo;
        bool uboDirty = true;
        ImageUbo uboData = {};

        std::vector<rhi::ImageViewPtr> gcObjects;

        rhi::ImagePtr dftImage;
        rhi::ImageViewPtr dftView;

        rhi::ImagePtr image;
        rhi::ImageViewPtr view;
        builder::ImageObjectPtr currentImage;
        std::vector<std::string> mipString;
        std::vector<const char*> mipStr;

        std::unordered_map<std::string, builder::CompressOption> compressPresets;
        std::unordered_map<std::string, builder::CompressedImagePtr> compressedImageObj;
        std::unordered_map<std::string, rhi::ImageViewPtr> compressedImageView;

        rhi::Sampler::Descriptor samplerDesc;
        rhi::SamplerPtr sampler;
        bool samplerDirty = false;
        bool resDirty = false;
        std::string currentPath;
        MD5 currentMd5;

        builder::MipGenType filterType = builder::MipGenType::Box;

        EventBinder<IWindowEvent> windowEvent;
        EventBinder<IMouseEvent> mouseEvent;
        EventBinder<IKeyboardEvent> keyboardEvent;

        bool showImageInfo = false;
        bool showSamplerInfo = false;
        bool showImageProcess = false;
        std::string currentPresetId;

        bool buttonDown[3] = {false};

        int clickedX = 0;
        int clickedY = 0;

        float lastX = 0.f;
        float lastY = 0.f;
    };

} // namespace sky
