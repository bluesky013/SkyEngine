//
// Created by blues on 2025/10/3.
//

#include "ImageRender.h"
#include <shader/ShaderCompilerGlsl.h>
#include <core/archive/FileArchive.h>
#include <builder/render/image/ImageLoader.h>
#include <builder/render/image/ImageMipGen.h>
#include <builder/render/image/ImageConverter.h>
#include <builder/render/image/ImageCompressor.h>
#include <rhi/Queue.h>
#include <core/math/Color.h>
#include <render/RHI.h>
#include "imgui.h"

namespace sky {
    extern std::unique_ptr<ShaderCompilerGlsl> GCompiler;;
    const char* MipFilter[] = {
        "Box", "Kaiser"
    };

    const char* CompressQuality[] = {
        "ULTRA_FAST",
        "VERY_FAST",
        "FAST",
        "BASIC",
        "SLOW"
    };

    const char* CompressFormat[] = {
        "BC7",
        "ASTC4x4",
        "ASTC8x8",
        "ASTC10x10",
    };

    rhi::PixelFormat CompressFormatTbl[] {
        rhi::PixelFormat::BC7_UNORM_BLOCK,
        rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK,
        rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK,
        rhi::PixelFormat::ASTC_10x10_UNORM_BLOCK
    };

    int32_t FindFmtIndex(rhi::PixelFormat fmt)
    {
        for (int32_t i = 0; i < IM_ARRAYSIZE(CompressFormatTbl); ++i) {
            if (CompressFormatTbl[i] == fmt) {
                return i;
            }
        }

        return 0;
    }

    static uint32_t CalculateHash(const std::string &path)
    {
        uint32_t res = 0;
        HashCombine32(res, Fnv1a32(path));
        return res;
    }

    Uuid CalculateGuidByPath(const std::string& path)
    {
        return Uuid::CreateWithSeed(CalculateHash(path));
    }

    static void CacheCompressedRawData(const std::string& path, const std::string& preset, const builder::CompressedImagePtr & obj, const MD5& md5)
    {
        FilePath filePath("ImageCache");
        filePath /= preset;
        filePath.MakeDirectory();

        filePath /= FilePath(CalculateGuidByPath(path).ToString());

        OFileArchive archive(filePath);
        archive.Save(md5.u64[0]);
        archive.Save(md5.u64[1]);
        obj->Save(archive);
    }

    builder::CompressedImagePtr LoadCompressedFromRawData(const std::string& path, const std::string& preset, const MD5& md5)
    {
        FilePath filePath("ImageCache");
        filePath /= preset;
        filePath /= FilePath(CalculateGuidByPath(path).ToString());

        IFileArchive archive(filePath);
        if (!archive.IsOpen()) {
            return {};
        }

        MD5 oldMd5;
        archive.Load(oldMd5.u64[0]);
        archive.Load(oldMd5.u64[1]);

        if (oldMd5 != md5) {
            return {};
        }

        builder::CompressedImagePtr image = std::make_shared<builder::CompressedImage>();
        if (image->Load(archive)) {
            return image;
        }

        return {};
    }

    static void CacheImageRawData(const std::string& path, const builder::ImageObjectPtr &obj, const MD5& md5)
    {
        FilePath filePath("ImageCache");
        filePath.MakeDirectory();

        filePath /= FilePath(CalculateGuidByPath(path).ToString());

        OFileArchive archive(filePath);
        archive.Save(md5.u64[0]);
        archive.Save(md5.u64[1]);
        obj->Save(archive);
    }

    static builder::ImageObjectPtr LoadImageFromRawData(const std::string& path, const MD5& md5)
    {
        FilePath filePath("ImageCache");
        filePath /= FilePath(CalculateGuidByPath(path).ToString());

        IFileArchive archive(filePath);
        if (!archive.IsOpen()) {
            return {};
        }

        MD5 oldMd5;
        archive.Load(oldMd5.u64[0]);
        archive.Load(oldMd5.u64[1]);

        if (oldMd5 != md5) {
            return {};
        }

        builder::ImageObjectPtr image = std::make_shared<builder::ImageObject>();
        if (image->Load(archive)) {
            return image;
        }

        return {};
    }

    ImageRender::ImageRender(rhi::Device* dev, NativeWindow* window,const rhi::RenderPassPtr &pass) : ImWidget("ImageRender"), device(dev)
    {
        windowEvent.Bind(this, window);
        mouseEvent.Bind(this);
        keyboardEvent.Bind(this);

        std::vector<rhi::ShaderPtr> shaders(2);
        rhi::Shader::Descriptor shaderDesc = {};

        ShaderSourceDesc desc = {};
        desc.entry = "main";

        ShaderCompileOption op = {ShaderCompileTarget::SPIRV, ShaderLanguage::GLSL};
        ShaderBuildResult result;
        {
            desc.source = ""
                          "layout(binding = 0) uniform Ubo {\n"
                          "    vec2 ScreenExt;\n"
                          "    vec2 ImageExt;\n"
                          "    vec2 Scale;\n"
                          "    vec2 Translate;\n"
                          "    uint level;\n"
                          "} pc;\n"
                          "layout(location = 0) out vec2 uv;\n"
                          "void main() {\n"
                          "    float x = float(gl_VertexIndex & 1);\n"
                          "    float y = float((gl_VertexIndex >> 1) & 1);\n"
                          "    uv = vec2(x, y);\n"
                          "    vec2 scale = pc.ScreenExt / pc.ImageExt * pc.Scale;\n"
                          "    vec2 offset = pc.Translate / pc.ScreenExt;\n"
                          "    gl_Position = vec4((uv * 2.0 - 1.0) * scale - offset * 2.0, 0.0, 1.0);\n"
                          "}";
            desc.stage = rhi::ShaderStageFlagBit::VS;
            result.data.clear();
            GCompiler->CompileBinary(desc, op, result);

            shaderDesc.data = reinterpret_cast<const uint8_t*>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size() * sizeof(uint32_t));
            shaderDesc.stage = desc.stage;
            shaders[0] = device->CreateShader(shaderDesc);
            shaders[0]->SetEntry("main");
        }

        {
            desc.source = ""
                          "layout(binding = 0) uniform Ubo {\n"
                          "    vec2 ScreenExt;\n"
                          "    vec2 ImageExt;\n"
                          "    vec2 Scale;\n"
                          "    vec2 Translate;\n"
                          "    uint level;\n"
                          "} pc;\n"
                          "layout(binding = 1) uniform sampler2D sTexture;\n"
                          "layout(location = 0) out vec4 color;\n"
                          "layout(location = 0) in vec2 uv;\n"
                          "void main() {\n"
                          "    color = textureLod(sTexture, uv, pc.level);\n"
                          "}";
            desc.stage = rhi::ShaderStageFlagBit::FS;
            result.data.clear();
            GCompiler->CompileBinary(desc, op, result);

            shaderDesc.data = reinterpret_cast<const uint8_t*>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size() * sizeof(uint32_t));
            shaderDesc.stage = desc.stage;
            shaders[1] = device->CreateShader(shaderDesc);
            shaders[1]->SetEntry("main");
        }

        rhi::DescriptorSetLayout::Descriptor setLayoutDesc = {};
        setLayoutDesc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding{rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS});
        setLayoutDesc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding{rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, 1, 1, rhi::ShaderStageFlagBit::FS});

        rhi::PipelineLayout::Descriptor layoutDesc = {};
        layoutDesc.layouts.emplace_back(device->CreateDescriptorSetLayout(setLayoutDesc));

        // rhi::VertexAttributeDesc vtxAttribute[3] = {
        //     {0, 0, 0, rhi::Format::F_RG32},
        //     {1, 0, 8, rhi::Format::U_R32},
        //     {2, 0, 12, rhi::Format::U_R32},
        // };
        // rhi::VertexBindingDesc vtxBinding = {0, sizeof(ImageQuadInstance), rhi::VertexInputRate::PER_INSTANCE};
        // rhi::VertexInput::Descriptor vertexInputDesc = {3, 1, vtxAttribute, &vtxBinding};

        layout = device->CreatePipelineLayout(layoutDesc);

        rhi::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.state.blendStates.emplace_back();
        psoDesc.state.inputAssembly.topology = rhi::PrimitiveTopology::TRIANGLE_STRIP;
        psoDesc.vs = shaders[0];
        psoDesc.fs = shaders[1];
        psoDesc.vertexInput = device->CreateVertexInput({});
        psoDesc.renderPass = pass;
        psoDesc.pipelineLayout = layout;

        pso = device->CreateGraphicsPipeline(psoDesc);

        rhi::DescriptorSetPool::PoolSize Sizes[2];
        Sizes[0] = {rhi::DescriptorType::UNIFORM_BUFFER, 1};
        Sizes[1] = {rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, 1};
        rhi::DescriptorSetPool::Descriptor poolDesc = {1, 2, Sizes};
        setPool = device->CreateDescriptorSetPool(poolDesc);

        rhi::DescriptorSet::Descriptor setDesc = {layoutDesc.layouts[0]};
        set = setPool->Allocate(setDesc);

        rhi::Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = sizeof(ImageUbo);
        bufferDesc.usage = rhi::BufferUsageFlagBit::UNIFORM;
        bufferDesc.memory = rhi::MemoryType::CPU_TO_GPU;
        ubo = device->CreateBuffer(bufferDesc);

        uboData.scale = VEC2_ONE;
        uboData.translate = VEC2_ZERO;
        uboData.screenExt = Vector2(static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()));
        uboData.imageExt = Vector2(2.f, 2.f);

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.imageType   = rhi::ImageType::IMAGE_2D;
        imageDesc.format      = rhi::PixelFormat::RGBA8_UNORM;
        imageDesc.extent      = {2, 2, 1};
        imageDesc.mipLevels   = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.samples     = rhi::SampleCount::X1;
        imageDesc.usage       = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::TRANSFER_DST;
        imageDesc.memory      = rhi::MemoryType::GPU_ONLY;

        dftImage = device->CreateImage(imageDesc);
        dftView = dftImage->CreateView({});

        samplerDesc.minFilter = rhi::Filter::NEAREST;
        samplerDesc.magFilter = rhi::Filter::NEAREST;
        samplerDesc.addressModeU = rhi::WrapMode::CLAMP_TO_BORDER;
        samplerDesc.addressModeV = rhi::WrapMode::CLAMP_TO_BORDER;
        samplerDesc.addressModeW = rhi::WrapMode::CLAMP_TO_BORDER;
        samplerDesc.maxLod = 1000.f;
        sampler = device->CreateSampler(samplerDesc);

        std::vector<uint8_t> color = {
            255, 255, 255, 255,   0,   0,   0, 255,
              0,   0,   0, 255, 255, 255, 255, 255
        };

        rhi::ImageUploadRequest uploadRequest = {};
        uploadRequest.size = color.size();
        uploadRequest.imageExtent.width = 2;
        uploadRequest.imageExtent.height = 2;
        uploadRequest.imageExtent.depth = 1;
        uploadRequest.source = new rhi::TRawBufferStream<uint8_t>(std::move(color));

        auto *queue = device->GetQueue(rhi::QueueType::TRANSFER);
        auto handle = queue->UploadImage(dftImage, uploadRequest);
        queue->Wait(handle);

        set->BindBuffer(0, ubo, 0, sizeof(ImageUbo), 0);
        set->BindImageView(1, dftView, 0);
        set->BindSampler(1, sampler, 0);
        set->Update();

        LoadConfigs();
    }

    ImageRender::~ImageRender()
    {
        pso = nullptr;
        layout = nullptr;
        setPool = nullptr;
        set = nullptr;
        image = nullptr;
        view = nullptr;
        dftImage = nullptr;
        dftView = nullptr;
        sampler = nullptr;

        SaveConfigs();
    }

    void ImageRender::LoadConfigs()
    {
        IFileArchive file(FilePath("image_tool.cfg"));
        if (!file.IsOpen()) {
            return;
        }

        JsonInputArchive json(file);

        auto num = json.StartArray("presets");
        for (uint32_t i = 0; i < num; ++i) {

            builder::CompressOption op = {};

            json.Start("name");
            std::string key = json.LoadString();
            json.End();

            json.Start("quality");
            op.quality = static_cast<builder::Quality>(json.LoadUint());
            json.End();

            json.Start("format");
            op.targetFormat = static_cast<rhi::PixelFormat>(json.LoadUint());
            json.End();

            json.Start("alpha");
            op.hasAlpha = json.LoadBool();
            json.End();

            json.NextArrayElement();

            compressPresets.emplace(key, op);
        }
        json.End();
    }

    void ImageRender::SaveConfigs()
    {
        OFileArchive file(FilePath("image_tool.cfg"));
        if (!file.IsOpen()) {
            return;
        }

        JsonOutputArchive json(file);

        json.StartObject();
        json.Key("presets");
        json.StartArray();

        for (auto &[key, preset] : compressPresets) {
            json.StartObject();

            json.Key("name");
            json.SaveValue(key);

            json.Key("quality");
            json.SaveValue(static_cast<uint32_t>(preset.quality));

            json.Key("format");
            json.SaveValue(static_cast<uint32_t>(preset.targetFormat));

            json.Key("alpha");
            json.SaveValue(preset.hasAlpha);

            json.EndObject();
        }
        json.EndArray();
        json.EndObject();
    }

    void ImageRender::GenerateMipMap()
    {
        if (!currentImage) {
            return;
        }

        currentImage->mips.resize(1);
        auto linearImage = builder::ImageObject::CreateImage2D(currentImage->width, currentImage->height, rhi::PixelFormat::RGBA8_UNORM);
        linearImage->FillMip0();
        {
            builder::ImageConverter converter(builder::ImageConverter::Payload{currentImage, linearImage, 2.2f});
            converter.DoWork();
        }

        {
            builder::ImageMipGen mipGen(builder::ImageMipGen::Payload{linearImage, filterType});
            mipGen.DoWork();
        }

        auto finalImage = builder::ImageObject::CreateFromImage(linearImage);
        {
            builder::ImageConverter converter(builder::ImageConverter::Payload{linearImage, finalImage, 1 / 2.2f});
            converter.DoWork();
        }

        UpdateImage(finalImage);
        CacheImageRawData(currentPath, finalImage, currentMd5);
    }

    void ImageRender::ShowOriginalImage()
    {
        set->BindImageView(1, view, 0);
        resDirty = true;
    }

    void ImageRender::ShowCompressImage(const std::string& key)
    {
        auto iter = compressedImageView.find(key);
        if (iter == compressedImageView.end() || !static_cast<bool>(iter->second)) {
            return;
        }

        set->BindImageView(1, iter->second, 0);
        resDirty = true;
    }

    void ImageRender::CompressImage(const std::string& key, const builder::CompressOption& option)
    {
        if (!currentImage) {
            return;
        }

        auto compressedImage = LoadCompressedFromRawData(currentPath, key, currentMd5);

        if (!compressedImage) {
            compressedImage = builder::CompressedImage::CreateFromImageObject(currentImage, option.targetFormat);

            for (uint32_t i = 0; i < currentImage->mips.size(); ++i) {
                builder::ImageCompressor op(builder::ImageCompressor::Payload{currentImage, compressedImage, option, i});
                op.DoWork();
            }

            CacheCompressedRawData(currentPath, key, compressedImage, currentMd5);
        }
        UpdateCompressedImage(key, compressedImage);
    }

    void ImageRender::UpdateCompressedImage(const std::string& key, const builder::CompressedImagePtr& imageObj)
    {
        {
            auto &compressedView = compressedImageView[key];
            if (compressedView) {
                gcObjects.emplace_back(compressedView);
            }

            compressedImageObj[key] = imageObj;
        }

        if (!device->CheckFormatFeature(imageObj->format, rhi::PixelFormatFeatureFlagBit::SAMPLE | rhi::PixelFormatFeatureFlagBit::SAMPLE_FILTER)) {
            return;
        }

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.imageType   = rhi::ImageType::IMAGE_2D;
        imageDesc.format      = imageObj->format;
        imageDesc.extent      = {imageObj->width, imageObj->height, 1};
        imageDesc.mipLevels   = static_cast<uint32_t>(imageObj->mips.size());
        imageDesc.arrayLayers = 1;
        imageDesc.samples     = rhi::SampleCount::X1;
        imageDesc.usage       = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::TRANSFER_DST;
        imageDesc.memory      = rhi::MemoryType::GPU_ONLY;

        auto compressedImage = device->CreateImage(imageDesc);

        rhi::ImageViewDesc viewDesc = {};
        viewDesc.subRange.aspectMask = rhi::AspectFlagBit::COLOR_BIT;
        viewDesc.subRange.baseLayer = 0;
        viewDesc.subRange.baseLevel = 0;
        viewDesc.subRange.levels = imageDesc.mipLevels;
        viewDesc.subRange.layers = imageDesc.arrayLayers;
        viewDesc.viewType = rhi::ImageViewType::VIEW_2D;

        compressedImageView[key] = compressedImage->CreateView(viewDesc);

        std::vector<rhi::ImageUploadRequest> requests;
        for (uint32_t i = 0; i < imageObj->mips.size(); ++i) {
            rhi::ImageUploadRequest imageRequest = {};
            imageRequest.source = new rhi::RawPtrStream(imageObj->mips[i].data.get());
            imageRequest.size = imageObj->mips[i].dataLength;
            imageRequest.imageExtent = imageDesc.extent;
            imageRequest.mipLevel = i;
            requests.emplace_back(imageRequest);
        }

        auto *queue = device->GetQueue(rhi::QueueType::TRANSFER);
        auto handle = queue->UploadImage(compressedImage, requests);
        queue->Wait(handle);
    }

    void ImageRender::UpdateImage(const builder::ImageObjectPtr& imageObj)
    {
        gcObjects.emplace_back(view);
        view = nullptr;
        image = nullptr;

        currentImage = imageObj;

        if (currentImage) {
            rhi::Image::Descriptor imageDesc = {};
            imageDesc.imageType   = imageObj->type;
            imageDesc.format      = imageObj->format;
            imageDesc.extent      = {imageObj->width, imageObj->height, imageObj->depth};
            imageDesc.mipLevels   = static_cast<uint32_t>(imageObj->mips.size());
            imageDesc.arrayLayers = 1;
            imageDesc.samples     = rhi::SampleCount::X1;
            imageDesc.usage       = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::TRANSFER_DST;
            imageDesc.memory      = rhi::MemoryType::GPU_ONLY;

            image = device->CreateImage(imageDesc);

            rhi::ImageViewDesc viewDesc = {};
            viewDesc.subRange.aspectMask = rhi::AspectFlagBit::COLOR_BIT;
            viewDesc.subRange.baseLayer = 0;
            viewDesc.subRange.baseLevel = 0;
            viewDesc.subRange.levels = imageDesc.mipLevels;
            viewDesc.subRange.layers = imageDesc.arrayLayers;
            viewDesc.viewType = rhi::ImageViewType::VIEW_2D;

            view = image->CreateView(viewDesc);

            UpdateImageExtent(imageObj->width, imageObj->height);

            std::vector<rhi::ImageUploadRequest> requests;
            for (uint32_t i = 0; i < imageObj->mips.size(); ++i) {
                rhi::ImageUploadRequest imageRequest = {};
                imageRequest.source = new rhi::RawPtrStream(imageObj->mips[i].data.get());
                imageRequest.size = imageObj->mips[i].dataLength;
                imageRequest.imageExtent = imageDesc.extent;
                imageRequest.mipLevel = i;
                requests.emplace_back(imageRequest);
            }

            auto *queue = device->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = queue->UploadImage(image, requests);
            queue->Wait(handle);

            set->BindImageView(1, view, 0);

            mipString.clear();
            mipStr.clear();
            mipString.resize(currentImage->mips.size());
            mipStr.resize(currentImage->mips.size());

            for (uint32_t i = 0; i < currentImage->mips.size(); ++i) {
                const auto& mip = currentImage->mips[i];

                std::stringstream ss;
                ss << mip.width << " x " << mip.height << "[mip " << i << " ]";

                mipString[i] = ss.str();
                mipStr[i] = mipString[i].c_str();
            }

        } else {
            set->BindImageView(1, dftView, 0);
        }

        resDirty = true;
    }

    void ImageRender::OnDropFile(const std::string& payload)
    {
        const FilePath path(payload);
        const std::string ext = path.Extension();
        const FilePtr imageFile = new NativeFile(path);
        const auto bin = imageFile->ReadBin();

        currentMd5 = MD5::CalculateMD5(reinterpret_cast<const char *>(bin->Data()), bin->Size());
        currentPath = payload;

        builder::ImageObjectPtr imageObject = LoadImageFromRawData(payload, currentMd5);

        if (!imageObject) {
            imageObject = builder::ImageLoaderManager::Get()->LoadImage(bin, ext);
            CacheImageRawData(currentPath, imageObject, currentMd5);
        }

        UpdateImage(imageObject);
    }

    void ImageRender::Render(rhi::GraphicsEncoder & encoder)
    {
        gcObjects.clear();

        if (uboDirty) {
            auto* ptr = ubo->Map();
            memcpy(ptr, &uboData, sizeof(ImageUbo));
            uboDirty = false;
        }

        if (samplerDirty) {
            auto newSampler = device->CreateSampler(samplerDesc);
            set->BindSampler(1, newSampler, 0);
            sampler = newSampler;

            samplerDirty = false;
            resDirty = true;
        }

        if (resDirty) {
            set->Update();
            resDirty = false;
        }

        encoder.BindPipeline(pso);
        encoder.BindSet(0, set);
        encoder.DrawLinear(rhi::CmdDrawLinear{4, 1, 0, 0});
    }

    void ImageRender::OnWindowResize(const WindowResizeEvent& event)
    {
        uboData.screenExt = Vector2(static_cast<float>(event.width), static_cast<float>(event.height));
        uboDirty = true;
    }

    void ImageRender::OnMouseWheel(const MouseWheelEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        if (event.y > 0) {
            uboData.scale *= Vector2(0.8f, 0.8f);
        } else if (event.y < 0){
            uboData.scale /= Vector2(0.8f, 0.8f);
        }

        uboDirty = true;
    }

    void ImageRender::OnMouseButtonDown(const MouseButtonEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        buttonDown[(uint32_t)event.button] = true;
        clickedX = event.x;
        clickedY = event.y;
    }

    void ImageRender::OnMouseButtonUp(const MouseButtonEvent &event)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        buttonDown[(uint32_t)event.button] = false;
        lastX = uboData.translate.x;
        lastY = uboData.translate.y;
    }

    void ImageRender::OnMouseMotion(const MouseMotionEvent &event)
    {
        if (buttonDown[0]) {
            int diffX = event.x - clickedX;
            int diffY = event.y - clickedY;

            Vector2 diff = Vector2(static_cast<float>(diffX), static_cast<float>(diffY));
            uboData.translate = Vector2(lastX, lastY) - diff;
            uboDirty = true;
        }
    }

    void ImageRender::OnTextInput(WindowID windID, const char *text)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharactersUTF8(text);
    }

    void ImageRender::UpdateImageExtent(uint32_t width, uint32_t height)
    {
        uboData.scale = Vector2(1.f, 1.f);
        uboData.imageExt = Vector2(static_cast<float>(width), static_cast<float>(height));
        uboDirty = true;
    }

    void ImageRender::DrawPresetSettingsPopup()
    {
        static int currentQuality = 0;
        static int currentFormat = 0;

        auto iter = compressPresets.find(currentPresetId);
        if (iter == compressPresets.end()) {
            return;
        }
        auto& op = iter->second;

        if (ImGui::BeginPopupModal("PresetSettings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::Text("Preset Name: %s", currentPresetId.c_str());
            ImGui::Separator();

            currentFormat = FindFmtIndex(op.targetFormat);
            if (ImGui::Combo("Format", &currentFormat, CompressFormat, IM_ARRAYSIZE(CompressFormat))) {
                op.targetFormat = CompressFormatTbl[currentFormat];
            }

            currentQuality = static_cast<int32_t>(op.quality);
            if (ImGui::Combo("Quality", &currentQuality, CompressQuality, IM_ARRAYSIZE(CompressQuality))) {
                op.quality = static_cast<builder::Quality>(currentQuality);
            }

            ImGui::Checkbox("Use Alpha Channel", &op.hasAlpha);

            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void ImageRender::Execute()
    {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Window")) {

                ImGui::MenuItem("Detail", nullptr, &showImageInfo);
                ImGui::MenuItem("Sampler", nullptr, &showSamplerInfo);
                ImGui::MenuItem("Process", nullptr, &showImageProcess);

                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (showSamplerInfo) {
            if (ImGui::Begin("SamplerInfo", nullptr)) {
                const char* items[] = { "Nearest", "Linear" };
                static int currentMinFilter = 0;
                static int currentMagFilter = 0;
                if (ImGui::Combo("Min Filter", &currentMinFilter, items, IM_ARRAYSIZE(items))) {
                    samplerDesc.minFilter = static_cast<rhi::Filter>(currentMinFilter);
                    samplerDirty = true;
                }

                if (ImGui::Combo("Mag Filter", &currentMagFilter, items, IM_ARRAYSIZE(items))) {
                    samplerDesc.magFilter = static_cast<rhi::Filter>(currentMagFilter);
                    samplerDirty = true;
                }

                ImGui::SameLine();
            }
            ImGui::End();
        }

        if (showImageInfo) {

            if (ImGui::Begin("ImageInfo", nullptr)) {
                const auto& desc = image ? image->GetDescriptor() : dftImage->GetDescriptor();

                ImGui::Text("width     :%u", desc.extent.width);
                ImGui::Text("height    :%u", desc.extent.height);
                ImGui::Text("mipLevels :%u", desc.mipLevels);
                ImGui::Text("slices    :%u", desc.arrayLayers);

                static int currentLevel = 0;

                if (ImGui::Combo("ShowMip", &currentLevel, mipStr.data(), static_cast<int>(mipStr.size()))) {
                    uboData.mipLevel = static_cast<uint32_t>(currentLevel);
                    uboDirty = true;
                }

            }
            ImGui::End();
        }

        if (showImageProcess) {

            static int currentFilter = 0;
            static bool showModal = false;
            std::string tobeDeleted;

            if (ImGui::Begin("ImageProcess", nullptr)) {

                if (ImGui::Combo("Filter", &currentFilter, MipFilter, IM_ARRAYSIZE(MipFilter))) {
                    filterType = static_cast<builder::MipGenType>(currentFilter);
                }

                if (ImGui::Button("Generate Mip Map")) {
                    GenerateMipMap();
                }

                ImGui::Separator();

                if (ImGui::BeginTable("PresetTable", 5, ImGuiTableFlags_Borders |
                                                            ImGuiTableFlags_RowBg |
                                                            ImGuiTableFlags_SizingFixedFit)) {
                    // 设置表头
                    ImGui::TableSetupColumn("Preset Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Generate", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableHeadersRow();

                    for (auto& [key, op] : compressPresets) {
                        ImGui::PushID(key.c_str());

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        ImGui::Text("%s", key.c_str());

                        ImGui::TableSetColumnIndex(1);
                        if (ImGui::Button("Gen")) {
                            CompressImage(key, op);
                        }

                        ImGui::TableSetColumnIndex(2);
                        if (ImGui::Button("Show")) {
                            ShowCompressImage(key);
                        }

                        ImGui::TableSetColumnIndex(3);
                        if (ImGui::Button("Edit")) {
                            currentPresetId = key;
                            showModal = true;
                        }

                        ImGui::TableSetColumnIndex(4);
                        if (ImGui::Button("-")) {
                            tobeDeleted = key;
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }

                if (ImGui::Button("Show Original")) {
                    ShowOriginalImage();
                }

                static char buffer[256] = "";
                ImGui::Text("Add New Preset");
                ImGui::InputTextWithHint("##newpreset", "Preset name...", buffer, IM_ARRAYSIZE(buffer));
                ImGui::SameLine();
                if (ImGui::Button("+") && buffer[0] != '\0') {
                    compressPresets.emplace(std::string(buffer), builder::CompressOption{});
                    buffer[0] = '\0';
                }
            }
            ImGui::End();

            if (showModal) {
                ImGui::OpenPopup("PresetSettings");
                showModal = false;
            }

            DrawPresetSettingsPopup();

            if (!tobeDeleted.empty()) {
                compressPresets.erase(tobeDeleted);
            }
        }
    }
} // namespace sky