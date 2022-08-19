//
// Created by Zach Lee on 2022/8/19.
//

#include <render/imgui/GuiManager.h>
#include <imgui.h>

namespace sky {

    void GuiManager::Init()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        VkDeviceSize fontImageSize = width * height * 4 * sizeof(char);

        Image::Descriptor imageDesc = {};
        imageDesc.extent.width = static_cast<uint32_t>(width);
        imageDesc.extent.height = static_cast<uint32_t>(height);
        imageDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
        fontImage = std::make_shared<Image>(imageDesc);
        fontImage->InitRHI();
        fontImage->Update(pixels, fontImageSize);

        Texture::Descriptor texDesc = {};
        fontTexture = Texture::CreateFromImage(fontImage, texDesc);
    }

    RDTexturePtr GuiManager::GetFontTexture()
    {
        return fontTexture;
    }
}